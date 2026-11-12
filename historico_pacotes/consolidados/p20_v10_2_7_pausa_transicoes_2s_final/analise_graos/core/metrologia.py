"""Estatisticas descritivas de leituras HX711; nao equivale a calibracao."""
import math
import statistics


def estatisticas_vetores(vetores):
    resultado = {}
    for etapa, leituras in vetores.items():
        massas = [float(x["peso_g"]) for x in leituras]
        if not massas:
            resultado[etapa] = {"n": 0, "media_g": None,
                                 "desvio_padrao_g": None, "amplitude_g": None}
            continue
        if not all(math.isfinite(v) for v in massas):
            raise ValueError(f"Valor nao finito no vetor {etapa}")
        resultado[etapa] = {
            "n": len(massas),
            "media_g": statistics.mean(massas),
            "desvio_padrao_g": statistics.stdev(massas) if len(massas) > 1 else None,
            "amplitude_g": max(massas)-min(massas),
            "min_g": min(massas), "max_g": max(massas),
        }
    return resultado
