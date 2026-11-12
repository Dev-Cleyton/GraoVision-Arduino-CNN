# -*- coding: utf-8 -*-
"""Diagnostico integral, SOMENTE LEITURA, Roboflow kernel-corn / 54.

Usa a chave ja existente num .env LOCAL. Consulta apenas api.roboflow.com
por HTTPS; GET para metadados e POST / exclusivamente para identificacao
do workspace (metodo de autenticacao usado pelo SDK oficial).
Nenhuma imagem, anotacao, peso de modelo, chave, conta ou JSON bruto e salvo.
"""
from __future__ import annotations

import argparse
from datetime import datetime, timezone
import json
import math
import os
from pathlib import Path
import re
import socket
import sys
from urllib.parse import urlencode, quote
from urllib.request import Request, HTTPRedirectHandler, build_opener
from urllib.error import HTTPError, URLError

BASE = "https://api.roboflow.com"
PROJECT = "kernel-corn"
VERSION = "54"
UA = "GraoVision-TCC-Auditoria-Metadados-ReadOnly/2.0"
MAX_BYTES = 6_000_000
MAX_EVALS = 20
BLOCK_KEYS = re.compile(r"(?i)(?:api.?key|secret|token|password|authorization|credential|signed|url|uri|path|email|user|member|billing|payment|private|owner|webhook|jwt|session|imageid|image_id|imageurl|annotationurl|weighturl)")
METRIC_KEYS = re.compile(r"(?i)(?:accuracy|precision|recall|f1|map|mAP|iou|metric|loss|epoch|batch|step|learning|lr\b|patience|size|width|height|split|count|images|annotations|classes|labels|train|valid|test|dataset|model|backbone|optimizer|architecture|augment|preprocess|threshold|confidence|overlap|resize|flop|parameters|distribution|health|quality|status|license|trained|date|created|updated|mosaic|task|format|export|anchor|checkpoint)")
STRING_KEYS = re.compile(r"(?i)^(?:type|modeltype|model_type|architecture|backbone|framework|status|task|annotation|format|license|optimizer|trainingtype|kind|version|modelgroup|model_family|modelFamily|image_size|resize|preprocessing|augmentation|classes|class|labels|split|tag|stage)$")
PROJECT_SAFE = re.compile(r"[A-Za-z0-9_-]{1,140}\Z")
ID_SAFE = re.compile(r"[A-Za-z0-9_.\-]{1,180}\Z")

class NoRedirect(HTTPRedirectHandler):
    def redirect_request(self, req, fp, code, msg, headers, newurl):
        return None

class ReadOnlyClient:
    def __init__(self, key, base=BASE, transport=None):
        self.key = key
        self.base = base
        self.transport = transport
        self.opener = build_opener(NoRedirect())
        self.calls = []

    def query(self, tag, path, params=None, auth=False):
        # Chave nunca logada. URLs geradas SOMENTE pelo codigo; host fixo.
        if not path.startswith("/") or "?" in path or "#" in path:
            raise ValueError("Caminho rejeitado por seguranca")
        opts = {"api_key": self.key}
        if params:
            opts.update({k: v for k, v in params.items() if v is not None})
        url = self.base + path + "?" + urlencode(opts)
        method = "POST" if auth else "GET"
        req = Request(url, data=b"" if auth else None, method=method,
                      headers={"Accept": "application/json", "User-Agent": UA})
        record = {"consulta": tag, "http": None, "resultado": None}
        data = None
        try:
            if self.transport is not None:
                status, data = self.transport(tag, path, opts, method)
            else:
                with self.opener.open(req, timeout=20) as res:
                    status = res.status
                    binary = res.read(MAX_BYTES + 1)
                    if len(binary) > MAX_BYTES:
                        raise ValueError("resposta maior que limite")
                    data = json.loads(binary.decode("utf-8"))
            record["http"] = int(status)
            if status == 200 and isinstance(data, (dict, list)):
                record["resultado"] = "OK"
            else:
                record["resultado"] = "sem JSON reconhecido"
                data = None
        except HTTPError as e:
            record["http"] = e.code
            record["resultado"] = status_error(e.code)
        except (URLError, TimeoutError, socket.timeout):
            record["resultado"] = "falha de rede ou TLS/timeout"
        except (ValueError, UnicodeError, json.JSONDecodeError):
            record["resultado"] = "formato desconhecido ou resposta excessiva"
        except Exception:
            record["resultado"] = "falha nao prevista, detalhes ocultos"
        self.calls.append(record)
        return data


def status_error(n):
    if n in (401, 403):
        return "acesso negado (permissao ou tipo de chave)"
    if n == 404:
        return "endpoint/recurso nao disponivel ou sem acesso"
    if n == 429:
        return "limite de consultas; resultado indisponivel"
    if n >= 500:
        return "falha temporaria do servidor"
    return "HTTP " + str(n)


def read_env(path):
    if not path.is_file():
        raise RuntimeError("Arquivo .env nao localizado: coloque o diagnostico junto ao .env do GraoVision")
    text = path.read_text("utf-8-sig")
    for name in ("ROBOFLOW_API_KEY", "ROBOFLOW_KEY"):
        for line in text.splitlines():
            s = line.strip()
            if not s or s.startswith("#"):
                continue
            if s.startswith("export "):
                s = s[7:].strip()
            k, sep, v = s.partition("=")
            if sep and k.strip() == name:
                v = v.strip()
                if v[:1] == v[-1:] and v.startswith(("'", '"')):
                    v = v[1:-1]
                if not v or v == "None" or re.search(r"\s", v):
                    raise RuntimeError("Chave ausente ou malformada no .env")
                return v
    raise RuntimeError("ROBOFLOW_API_KEY nao encontrada no .env. Nao cole a chave no terminal.")


def find_env(scriptdir, specified=None):
    candidates = [Path(specified)] if specified else [
        scriptdir / ".env", scriptdir / "analise_graos" / ".env",
        scriptdir.parent / "analise_graos" / ".env",
        scriptdir.parent / ".env", Path.cwd() / ".env",
    ]
    for p in candidates:
        if p.is_file():
            return p.resolve()
    raise RuntimeError("Nenhum .env encontrado. Copie este programa para a pasta que possui o .env do GraoVision.")


def find_ws(auth):
    if not isinstance(auth, dict):
        return None
    a = auth.get("workspace")
    if isinstance(a, dict):
        a = a.get("url") or a.get("slug")
    if isinstance(a, str) and PROJECT_SAFE.fullmatch(a):
        return a
    return None


def sanitize_str(s, field):
    if not isinstance(s, str):
        return None
    if BLOCK_KEYS.search(field) or len(s) > 90:
        return None
    if any(t in s.lower() for t in ("http", "mailto:", "bearer ", "api_key", "sk_", "rf_", "/", "\\", "@", "?", "=", "&")):
        return None
    if not re.fullmatch(r"[a-zA-Z0-9À-ÿ _.,:;%()+\-]{1,90}", s):
        return None
    # Evita copiar strings de alta entropia, comuns em IDs e tokens.
    if len(s) > 36 and " " not in s:
        return None
    return s


def pick_interesting(payload, name, max_rows=240):
    """Nunca persiste JSON de resposta. Retem apenas campos tecnicos filtrados.
    Arrays longos: extremos e maior valor em metricas, nao registros individuais.
    """
    rows = []
    def visit(node, prefix="", depth=0):
        if depth > 12 or len(rows) >= max_rows:
            return
        if isinstance(node, dict):
            for k, v in list(node.items())[:180]:
                key = str(k)
                if BLOCK_KEYS.search(key) or len(key) > 80:
                    continue
                keypath = prefix + "." + key if prefix else key
                if isinstance(v, (dict, list)):
                    visit(v, keypath, depth + 1)
                elif isinstance(v, bool):
                    if METRIC_KEYS.search(key):
                        rows.append((keypath, v))
                elif isinstance(v, (int, float)) and not isinstance(v, bool) and math.isfinite(v):
                    if METRIC_KEYS.search(key) or METRIC_KEYS.search(prefix):
                        rows.append((keypath, v))
                elif isinstance(v, str) and (STRING_KEYS.fullmatch(key) or (key.lower() in ("modeltype", "model_type", "status"))):
                    safe = sanitize_str(v, key)
                    if safe is not None:
                        rows.append((keypath, safe))
        elif isinstance(node, list):
            if len(node) > 20:
                rows.append((prefix + ".quantidade_elementos", len(node)))
                ix = sorted(set([0, 1, len(node)//2, len(node)-2, len(node)-1]))
            else:
                ix = list(range(len(node)))
            for i in ix:
                item = node[i]
                if isinstance(item, str) and re.search(r"(?:classes|labels|categories)$", prefix, re.I):
                    safe = sanitize_str(item, "class")
                    if safe is not None:
                        rows.append((prefix + "[" + str(i) + "]", safe))
                else:
                    visit(item, prefix + "[" + str(i) + "]", depth + 1)
    visit(payload)
    return rows[:max_rows]


def show_structure(payload, max_paths=500):
    """Mostra nomes de campos sem valores, para identificar campos nao reconhecidos."""
    found = []
    def visit(node, prefix="", depth=0):
        if depth > 9 or len(found) >= max_paths:
            return
        if isinstance(node, dict):
            for k, v in list(node.items())[:160]:
                k = str(k)
                if BLOCK_KEYS.search(k):
                    continue
                path = prefix + "." + k if prefix else k
                if METRIC_KEYS.search(k):
                    found.append(path)
                if isinstance(v, (dict, list)):
                    visit(v, path, depth+1)
        elif isinstance(node, list) and node:
            visit(node[0], prefix + "[]", depth+1)
    visit(payload)
    return found[:max_paths]


def find_status(payload, path):
    obj = payload
    for k in path:
        if isinstance(obj, dict):
            obj = obj.get(k)
        else:
            return None
    return obj


def numeric_value_for(payload, key):
    """Procura recursivamente contagens; mantem caminhos para nao confundir escopos."""
    found = []
    def visit(obj, prefix="", depth=0):
        if depth > 7 or len(found) > 100:
            return
        if isinstance(obj, dict):
            for k, v in obj.items():
                kp = str(k)
                if BLOCK_KEYS.search(kp):
                    continue
                path = prefix + "." + kp if prefix else kp
                if re.search(key, kp, re.I) and isinstance(v, (int, float)) and not isinstance(v, bool):
                    found.append((path, v))
                if isinstance(v, dict):
                    visit(v, path, depth+1)
        elif isinstance(obj, list) and len(obj) < 15:
            for i, v in enumerate(obj):
                if isinstance(v, dict):
                    visit(v, prefix+"["+str(i)+"]", depth+1)
    visit(payload)
    return found


def build_report(project, version, results, client):
    sections = [
        "DIAGNOSTICO INTEGRAL — ROBOFLOW | kernel-corn / versao 54",
        "Gerado em: " + datetime.now(timezone.utc).strftime("%Y-%m-%d %H:%M UTC"),
        "Consulta SOMENTE DE LEITURA. Nenhuma imagem, peso ou anotacao baixado.",
        "Uma chave de inferencia pode nao dar acesso ao conjunto de treinamento.",
        "",
        "A) STATUS DE TODAS AS CONSULTAS",
    ]
    for q in client.calls:
        sections.append("- {}: {} | {}".format(q["consulta"], str(q["http"]) if q["http"] else "sem HTTP", q["resultado"]))
    sections += ["", "B) RESULTADOS TECNICOS POR FONTE"]
    for entry in results:
        sections += ["", "## " + entry["nome"]]
        sections.append("Situação: " + ("DADOS RETORNADOS" if entry["disponivel"] else "NAO DISPONIVEL VIA ESTA CHAVE/CONSULTA"))
        if entry["disponivel"]:
            if entry["dados"]:
                for path, value in entry["dados"]:
                    sections.append("- " + str(path) + " = " + str(value))
            else:
                sections.append("- A API respondeu, mas nenhum campo tecnico do filtro foi encontrado.")
            if entry["paths"]:
                sections.append("Campos tecnicos presentes (nomes, sem valores): " + ", ".join(entry["paths"][:80]))
    sections += ["", "C) INTERPRETACAO E LIMITES"]
    sections += [
        "- As contagens do projeto podem diferir das contagens da versao 54 (geracao, duplicacao, splits).",
        "- Na deteccao, 'bom/ruim/misto' na imagem nao corresponde necessariamente a tres classes de objeto.",
        "- METRICAS DO TREINAMENTO e METRICAS DE AVALIACAO EXTERNA sao fontes distintas; nao confundir.",
        "- Campo vazio/HTTP 403/404 NAO prova que uma metrica nunca foi calculada.",
        "- Nenhuma metrica deve ser inventada a partir de apenas contagens de inferencia.",
        "- Nenhum arquivo .env, chave, imagem, anotacao, URL assinada ou JSON bruto foi salvo.",
        "- O modo de autenticacao usa POST / apenas para consulta do workspace (sem efeito de escrita).",
        "", "D) RESUMO PARA A BANCA",
    ]
    if isinstance(version, dict) and any(str(v).lower() == "yolov8n" for k,v in pick_interesting(version, "ver", 300) if "modeltype" in k.lower() or "model_type" in k.lower()):
        sections.append("- YOLOv8n identificado por metadados da versao 54.")
    else:
        sections.append("- Familia do modelo deve ser confirmada pelos campos de B). Diagnostico anterior encontrou yolov8n.")
    ds = [x for x in results if x["nome"] in ("Projeto", "Versao 54")]
    if any(any(re.search(r"(?:train|valid|test|split|images)", p, re.I) for p,v in r["dados"]) for r in ds):
        sections.append("- Campos de imagens/splits podem estar disponiveis em B), com escopo indicado pelo caminho.")
    else:
        sections.append("- Contagens e divisao de imagens nao foram retornadas nesta consulta.")
    if any(any(re.search(r"(?:map|precision|recall|accuracy|f1)", p, re.I) for p,v in r["dados"]) for r in results):
        sections.append("- Campos de metrica encontrados em B); interpretar de acordo com treino/avaliacao e split.")
    else:
        sections.append("- Nao foram retornados numeros confiaveis de precision/recall/mAP.")
    sections.append("- Antes de citar valores no TCC, confira se pertencem a kernel-corn/54, a qual split e qual modelo.")
    return "\n".join(sections) + "\n"


def execute(envfile=None, simulate=None, quiet=False):
    basedir = Path(__file__).resolve().parent
    key = "CHAVE_FAKE_PARA_TESTES" if simulate else read_env(find_env(basedir, envfile))
    client = ReadOnlyClient(key, transport=simulate)
    all_data = []
    def collect(label, path, params=None, auth=False):
        if not quiet:
            print("Consultando:", label, flush=True)
        payload = client.query(label, path, params, auth)
        all_data.append({"nome": label, "disponivel": payload is not None,
                         "dados": pick_interesting(payload, label) if payload is not None else [],
                         "paths": show_structure(payload) if payload is not None else []})
        return payload

    auth = collect("Autenticacao / workspace", "/", auth=True)
    ws = find_ws(auth)
    if not ws:
        raise RuntimeError("Nao foi possivel identificar o workspace. Confira o acesso da chave; nenhuma outra consulta foi feita.")
    safe_ws = quote(ws, safe="")
    pp = "/" + safe_ws + "/" + PROJECT
    p = collect("Projeto", pp)
    v = collect("Versao 54", pp + "/" + VERSION)
    if not p or not v:
        raise RuntimeError("Projeto ou versao indisponivel; nenhuma informacao de outra conta sera consultada.")
    # Escopo explicitamente limitado ao projeto/version consultados.
    collect("Saude do conjunto de imagens (sem regenerar)", pp + "/health")
    collect("Modelos vinculados ao projeto", pp + "/models")
    training_list = collect("Treinamentos da versao 54", pp + "/" + VERSION + "/v2/trainings")
    collect("Resultado consolidado do treinamento", pp + "/" + VERSION + "/training/results")
    collect("Detalhes do treinamento padrao", pp + "/" + VERSION + "/v2/trainings/get")
    runs = training_list.get("trainings", []) if isinstance(training_list, dict) else []
    if isinstance(runs, list):
        for index, run in enumerate(runs[:20]):
            tid = run.get("id") if isinstance(run, dict) else None
            if isinstance(tid, str) and len(tid) <= 180 and re.fullmatch(r"[a-zA-Z0-9_./:-]+", tid):
                # IDs de treino legados podem conter barras: usar parametro de query seguro.
                collect("Treinamento especifico {}".format(index+1),
                        pp + "/" + VERSION + "/v2/trainings/get", {"trainingId": tid})
    # Lista de avaliacoes, se a conta possuir permissoes e o recurso existir.
    evals = collect("Avaliacoes existentes", "/" + safe_ws + "/model-evals",
                    {"project": PROJECT, "version": VERSION, "limit": 100})
    eval_ids = []
    if isinstance(evals, dict):
        for container in ("evaluations", "evals", "results", "items"):
            vals = evals.get(container)
            if isinstance(vals, list):
                for e in vals:
                    if isinstance(e, dict):
                        a = e.get("id") or e.get("evalId")
                        if isinstance(a, str) and ID_SAFE.fullmatch(a):
                            eval_ids.append(a)
                break
    # Resumo detalhado de cada avaliacao, se existir; limite para evitar chamadas excessivas.
    for index, eval_id in enumerate(dict.fromkeys(eval_ids)):
        if index >= MAX_EVALS:
            break
        base_eval = "/" + safe_ws + "/model-evals/" + quote(eval_id, safe="")
        collect("Avaliacao {} — resumo".format(index+1), base_eval)
        collect("Avaliacao {} — mAP por divisao".format(index+1), base_eval + "/map-results")
        collect("Avaliacao {} — precisao/recall/F1".format(index+1), base_eval + "/confidence-sweep")
        collect("Avaliacao {} — desempenho por classe".format(index+1), base_eval + "/performance-by-class")
        collect("Avaliacao {} — matriz de confusao".format(index+1), base_eval + "/confusion-matrix")
    report = build_report(p, v, all_data, client)
    safe_json = {"projeto": PROJECT, "versao": VERSION, "gerado_utc": datetime.now(timezone.utc).isoformat(),
                 "consultas": client.calls, "secoes": [
                    {"nome": x["nome"], "disponivel": x["disponivel"],
                     "dados": [{"campo": k, "valor": val} for k, val in x["dados"]],
                     "campos_disponiveis": x["paths"][:200]} for x in all_data]}
    f_txt = basedir / "RESULTADO_COMPLETO_ROBOFLOW_V54_SEM_CHAVE.txt"
    f_json = basedir / "RESULTADO_ESTRUTURADO_ROBOFLOW_V54_SEM_CHAVE.json"
    f_txt.write_text(report, encoding="utf-8")
    f_json.write_text(json.dumps(safe_json, ensure_ascii=False, indent=2), encoding="utf-8")
    if not quiet:
        print("\nConsulta concluida. Arquivos produzidos:")
        print(" -", f_txt.name)
        print(" -", f_json.name)
        print("Somente os dois RESULTADOS podem ser compartilhados. Nunca envie seu .env.")
    return report, safe_json


def main():
    parser = argparse.ArgumentParser(description="Inventario de metadados Roboflow v54, sem alteracoes")
    parser.add_argument("--env", help="Caminho local para o arquivo .env do GraoVision")
    args = parser.parse_args()
    print("=== Diagnostico integral Roboflow v54 | somente metadados ===", flush=True)
    try:
        execute(args.env)
        return 0
    except (KeyboardInterrupt, SystemExit):
        print("Interrompido; nenhum recurso remoto foi alterado.")
        return 2
    except Exception:
        # Nao imprime traceback nem URL por conterem parametros de autenticacao.
        print("Consulta incompleta. Verifique .env, acesso e conectividade; detalhes sensiveis ocultados.")
        return 1

if __name__ == "__main__":
    sys.exit(main())
