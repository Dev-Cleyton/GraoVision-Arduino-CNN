# -*- coding: utf-8 -*-
"""Consulta de leitura dos metadados do projeto kernel-corn, versao 54.

Nao acessa Arduino, nao modifica projetos, nao faz treinamento ou inferencia.
A chave e carregada do .env local, nunca impressa nem gravada no resultado.
Somente dominios HTTPS oficiais do Roboflow sao contatados.
"""

import json
import os
import re
import sys
import urllib.error
import urllib.parse
import urllib.request
from pathlib import Path

PROJETO = "kernel-corn"
VERSAO = 54
API_BASE = "https://api.roboflow.com"
ARQUIVO_RESULTADO = "resultado_diagnostico_roboflow_v54.txt"
MAX_RESPOSTA_BYTES = 2_000_000


class ErroDiagnostico(Exception):
    pass


class SemRedirecionamento(urllib.request.HTTPRedirectHandler):
    """Nao encaminhar chave acidentalmente a qualquer outro servidor."""

    def redirect_request(self, req, fp, code, msg, headers, newurl):
        raise ErroDiagnostico("Servidor solicitou redirecionamento; consulta interrompida por seguranca.")


def ler_chave_dotenv(arquivo: Path):
    if not arquivo.is_file():
        raise ErroDiagnostico(
            "Arquivo .env nao encontrado. Coloque este script na pasta analise_graos, "
            "junto do .env que o GrãoVision ja utiliza."
        )
    for linha in arquivo.read_text(encoding="utf-8-sig").splitlines():
        linha = linha.strip()
        if not linha or linha.startswith("#"):
            continue
        if linha.startswith("export "):
            linha = linha[len("export "):].strip()
        chave, sep, valor = linha.partition("=")
        if sep and chave.strip() == "ROBOFLOW_API_KEY":
            valor = valor.strip()
            if len(valor) >= 2 and valor[0] == valor[-1] and valor[0] in ('"', "'"):
                valor = valor[1:-1]
            elif " #" in valor:
                valor = valor.split(" #", 1)[0].strip()
            if not valor or valor.startswith("COLOQUE_SUA_CHAVE"):
                raise ErroDiagnostico("ROBOFLOW_API_KEY nao esta preenchida no .env.")
            if any(c.isspace() for c in valor):
                raise ErroDiagnostico("ROBOFLOW_API_KEY contem espacos inesperados.")
            return valor
    raise ErroDiagnostico("Variavel ROBOFLOW_API_KEY nao localizada no .env.")


def consulta_api(caminho, chave, metodo="GET"):
    if not caminho.startswith("/"):
        raise ErroDiagnostico("Caminho da API invalido.")
    parametros = urllib.parse.urlencode({"api_key": chave})
    url = API_BASE + caminho + "?" + parametros
    req = urllib.request.Request(
        url,
        data=b"" if metodo == "POST" else None,
        method=metodo,
        headers={"Accept": "application/json", "User-Agent": "GraoVision-ReadOnly-Diagnostic/1.0"},
    )
    try:
        with urllib.request.build_opener(SemRedirecionamento()).open(req, timeout=15) as resposta:
            bruto = resposta.read(MAX_RESPOSTA_BYTES + 1)
            if len(bruto) > MAX_RESPOSTA_BYTES:
                raise ErroDiagnostico("Resposta excessivamente grande, consulta interrompida.")
            return json.loads(bruto.decode("utf-8"))
    except urllib.error.HTTPError as exc:
        # Nao imprimir exc, URL nem corpo: alguns erros de API podem ecoar a chave.
        if exc.code in (401, 403):
            raise ErroDiagnostico(
                "API recusou a consulta (HTTP %s). A chave pode autorizar inferencia, "
                "mas nao leitura dos metadados." % exc.code
            ) from None
        if exc.code == 404:
            raise ErroDiagnostico("Recurso nao encontrado (HTTP 404). Verifique acesso ao projeto e versao.") from None
        raise ErroDiagnostico("API retornou HTTP %s. Nenhuma alteracao foi realizada." % exc.code) from None
    except urllib.error.URLError:
        raise ErroDiagnostico("Falha de rede ou TLS. Verifique a internet e tente novamente.") from None
    except TimeoutError:
        raise ErroDiagnostico("A consulta excedeu o tempo limite.") from None
    except (UnicodeDecodeError, json.JSONDecodeError):
        raise ErroDiagnostico("A API retornou formato nao reconhecido.") from None


def slug_valido(valor):
    """Evita query injection/caminhos arbitrarios recebidos da API."""
    return isinstance(valor, str) and bool(re.fullmatch(r"[a-zA-Z0-9_-]{1,140}", valor))


def achar_campos(dic, caminho="", nivel=0):
    """Filtra SÓ nomes de campos tecnicos; nao despeja o JSON ou dados privados."""
    whitelist = {
        "modeltype", "model_type", "architecture", "architecturetype", "backbone",
        "modelarchitecture", "modelfamily", "model_family", "framework", "trainingtype",
    }
    encontrados = []
    if nivel > 8:
        return encontrados
    if isinstance(dic, dict):
        for k, v in dic.items():
            nome = str(k)
            subcaminho = (caminho + "." + nome) if caminho else nome
            if nome.replace("-", "_").lower() in whitelist:
                if isinstance(v, (str, int, float, bool)) and not isinstance(v, bool):
                    valor = str(v)
                    # Valor esperado e nome de arquitetura curto, sem strings extensas.
                    if len(valor) <= 120 and re.fullmatch(r"[a-zA-Z0-9_.:/+() -]+", valor):
                        encontrados.append((subcaminho, valor))
            if isinstance(v, (dict, list)):
                encontrados.extend(achar_campos(v, subcaminho, nivel + 1))
    elif isinstance(dic, list):
        for i, v in enumerate(dic[:30]):
            encontrados.extend(achar_campos(v, f"{caminho}[{i}]", nivel + 1))
    return encontrados


def interpretar_resultados(campos):
    txt = " ".join(v.lower() for _, v in campos)
    if "rf-detr" in txt or "rfdetr" in txt:
        return "Ha referencia a RF-DETR; nao descrever como CNN sem conferir documentacao da arquitetura."
    if re.search(r"\byolo(v|x|\d|[-_])", txt) or re.search(r"\byolov\d", txt):
        return "Ha referencia a familia YOLO; versao e arquitetura devem seguir o valor retornado."
    if "efficientdet" in txt or "mobilenet" in txt or "resnet" in txt or "fasterrcnn" in txt:
        return "Ha indicacao de uma familia de arquitetura; confira o valor original antes de citar."
    return "Arquitetura interna ainda NAO confirmada por estes metadados."


def construir_relatorio(workspace, projeto_info, versao_info):
    projeto = projeto_info.get("project", {}) if isinstance(projeto_info, dict) else {}
    versao = versao_info.get("version", {}) if isinstance(versao_info, dict) else {}
    if not isinstance(projeto, dict) or not isinstance(versao, dict):
        raise ErroDiagnostico("Resposta da API nao contem estruturas de projeto/versao esperadas.")
    # A chave, identificador da conta e JSON bruto NUNCA sao incluidos.
    tarefa = projeto.get("type")
    if not isinstance(tarefa, str) or len(tarefa) > 80:
        tarefa = "Nao informada"
    campos = achar_campos(versao, "version")
    if not campos:
        campos = achar_campos(projeto, "project")
    saida = [
        "DIAGNOSTICO ROBOFLOW — CONSULTA SOMENTE DE LEITURA",
        "Projeto consultado: " + PROJETO,
        "Versao consultada: " + str(VERSAO),
        "Consulta de metadados: autorizada pela API (resposta recebida)",
        "Tipo de tarefa do projeto: " + tarefa,
        "",
        "METADADOS DE ARQUITETURA DISPONIVEIS:",
    ]
    if campos:
        for k, v in campos[:25]:
            saida.append("- " + k + " = " + v)
    else:
        saida.append("- Nenhum campo de arquitetura retornado.")
    saida.extend([
        "",
        "INTERPRETACAO:",
        interpretar_resultados(campos),
        "O tipo de tarefa (ex.: object-detection) NAO identifica a arquitetura da rede.",
        "",
        "PRIVACIDADE E LIMITES:",
        "A chave da API NAO foi salva neste relatorio.",
        "Nao foram enviados arquivos de amostras nem feitas alteracoes no Roboflow.",
        "Um resultado sem modelType nao prova que o modelo nao utiliza CNN.",
    ])
    return "\n".join(saida) + "\n"


def main():
    print("=== Diagnostico Roboflow kernel-corn v54 — somente leitura ===")
    print("Busca .env na mesma pasta deste arquivo. Nunca exibe a chave.")
    script_dir = Path(__file__).resolve().parent
    try:
        key = ler_chave_dotenv(script_dir / ".env")
        # Endpoint de validacao do SDK do Roboflow usa POST / sem alterar recursos.
        autenticacao = consulta_api("/", key, metodo="POST")
        ws = autenticacao.get("workspace") if isinstance(autenticacao, dict) else None
        if isinstance(ws, dict):
            ws = ws.get("url")
        if not slug_valido(ws):
            raise ErroDiagnostico("A API nao informou um workspace reconhecivel para a chave.")
        proj = consulta_api("/" + ws + "/" + PROJETO, key)
        versao = consulta_api("/" + ws + "/" + PROJETO + "/" + str(VERSAO), key)
        rel = construir_relatorio(ws, proj, versao)
        caminho = script_dir / ARQUIVO_RESULTADO
        caminho.write_text(rel, encoding="utf-8")
        print("\n" + rel)
        print("Resultado salvo em:", caminho.name)
        print("Este arquivo de resultado pode ser compartilhado sem incluir a chave.")
        return 0
    except ErroDiagnostico as exc:
        print("\nRESULTADO: " + str(exc))
        print("Chave nao exibida. Nenhuma mudanca realizada no Roboflow.")
        return 1
    except Exception:
        # Evita vazar URL ou segredos de exceptions inesperadas.
        print("\nRESULTADO: Erro nao previsto durante a consulta; detalhes ocultados por seguranca.")
        return 1


if __name__ == "__main__":
    sys.exit(main())
