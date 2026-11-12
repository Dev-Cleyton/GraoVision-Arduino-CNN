import csv
import os
from collections import Counter

import cv2
from roboflow import Roboflow


class GrainClassifier:
    """Diagnostico de deteccoes sem alterar a API publica classificar(sample)."""

    # Coordenadas relativas a imagem COMPLETA. Ajustar se a camera mudar.
    ROI_CX = 0.49
    ROI_CY = 0.50
    ROI_RAIO_ALTURA = 0.35
    CONFIANCA = 28
    OVERLAP = 24

    def __init__(self, api_key, project="kernel-corn", version=54):
        if not api_key:
            raise ValueError("ROBOFLOW_API_KEY nao configurada no .env")
        rf = Roboflow(api_key=api_key)
        self.model = rf.workspace().project(project).version(version).model

    def classificar(self, sample):
        total_graos = 0
        graos_ruins = 0
        os.makedirs("imagens", exist_ok=True)

        if len(sample.imagens) != 1:
            print("[IA][AVISO] Esperada uma unica imagem; resultados somam imagens diferentes.")

        for indice, caminho in enumerate(sample.imagens, 1):
            imagem = cv2.imread(caminho)
            if imagem is None:
                raise RuntimeError(f"Nao foi possivel ler a imagem: {caminho}")
            altura, largura = imagem.shape[:2]
            cx = int(largura * self.ROI_CX)
            cy = int(altura * self.ROI_CY)
            raio = int(altura * self.ROI_RAIO_ALTURA)
            cv2.circle(imagem, (cx, cy), raio, (255, 255, 0), 3)

            resultado = self.model.predict(
                caminho, confidence=self.CONFIANCA, overlap=self.OVERLAP
            ).json()
            predicoes = resultado.get("predictions", [])
            classes = Counter()
            aceitas = 0
            descartadas = 0
            ruins = 0
            linhas_csv = []

            for numero, pred in enumerate(predicoes, 1):
                try:
                    x = float(pred["x"])
                    y = float(pred["y"])
                    w = float(pred["width"])
                    h = float(pred["height"])
                except (KeyError, ValueError, TypeError):
                    print(f"[IA][AVISO] Predicao {numero} sem coordenadas validas; ignorada")
                    continue
                classe = str(pred.get("class", "DESCONHECIDA"))
                confianca = float(pred.get("confidence", 0))
                classes[classe] += 1
                dentro = (x - cx) ** 2 + (y - cy) ** 2 <= raio ** 2
                if dentro:
                    aceitas += 1
                    if classe.lower() == "bad":
                        ruins += 1
                else:
                    descartadas += 1

                x1, y1 = int(x - w / 2), int(y - h / 2)
                x2, y2 = int(x + w / 2), int(y + h / 2)
                cor = (0, 0, 255) if dentro and classe.lower() == "bad" else (
                    (0, 180, 0) if dentro else (130, 130, 130)
                )
                cv2.rectangle(imagem, (x1, y1), (x2, y2), cor, 2)
                rotulo = f"{classe} {confianca:.2f}" + (" FORA" if not dentro else "")
                cv2.putText(imagem, rotulo, (max(0, x1), max(20, y1 - 6)),
                            cv2.FONT_HERSHEY_SIMPLEX, 0.48, cor, 2)
                linhas_csv.append([numero, classe, round(confianca, 4),
                                   round(x, 1), round(y, 1), round(w, 1),
                                   round(h, 1), "SIM" if dentro else "NAO"])

            nome_base = os.path.splitext(os.path.basename(caminho))[0]
            pasta = os.path.dirname(caminho) or "."
            anotada = os.path.join(pasta, f"{nome_base}_detectada.jpg")
            csv_saida = os.path.join(pasta, f"{nome_base}_deteccoes.csv")
            if not cv2.imwrite(anotada, imagem):
                raise RuntimeError(f"Falha ao salvar imagem anotada: {anotada}")
            with open(csv_saida, "w", newline="", encoding="utf-8-sig") as arquivo:
                writer = csv.writer(arquivo, delimiter=";")
                writer.writerow(["id", "classe", "confianca", "x", "y", "largura",
                                 "altura", "dentro_roi"])
                writer.writerows(linhas_csv)

            total_graos += aceitas
            graos_ruins += ruins
            print(f"[IA] Imagem {indice}: {len(predicoes)} deteccoes brutas; "
                  f"{aceitas} dentro da bandeja; {descartadas} fora")
            print(f"[IA] Classes retornadas: {dict(classes)}")
            print(f"[IA] Grupos 'bad' dentro da ROI: {ruins}")
            print(f"[IA] Imagem anotada: {anotada}")
            print(f"[IA] Planilha de deteccoes: {csv_saida}")

        sample.total_graos = total_graos
        sample.graos_ruins = graos_ruins
        sample.percentual_ruins = (
            100.0 * graos_ruins / total_graos if total_graos else 0.0
        )
        print("[IA][ATENCAO] Contagens sao DETECCOES, ainda nao validadas "
              "contra a contagem manual. A ROI nao elimina falsos positivos internos.")
        return sample
