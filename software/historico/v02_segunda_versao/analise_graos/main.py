import os

from core.analysis_service import AnalysisService

from dotenv import load_dotenv

load_dotenv()

print("Roboflow: credencial nao exibida")

def exibir_resultados(sessao):

    """
    Exibe os resultados da análise.
    """

    print("\n" + "=" * 50)
    print("RESULTADO DA ANÁLISE")
    print("=" * 50)

    # Verifica se ocorreu algum erro
    if sessao.possui_erro:

        print(f"\nERRO: {sessao.mensagem_erro}")

        return

    sample = sessao.sample

    print(f"\nTotal de grãos...............: {sample.total_graos}")
    print(f"Grãos ruins..................: {sample.graos_ruins}")
    print(f"Percentual de defeitos.......: {sample.percentual_ruins:.2f}%")

    print(f"\nPeso inicial.................: {sample.peso_inicial}")
    print(f"Peso final...................: {sample.peso_final}")

    print(f"\nTemperatura..................: {sample.temperatura}")
    print(f"Umidade......................: {sample.umidade}")

    print(f"\nAmostra aprovada.............: {sample.aprovado}")

    print("\nStatus da sessão.............:", sessao.status)

    print("=" * 50)


def executar_sistema():

    """
    Executa o sistema continuamente até o usuário
    decidir encerrá-lo.
    """

    while True:

        try:

            analysis_service = AnalysisService()

            sessao = analysis_service.executar()

            exibir_resultados(sessao)

        except Exception as erro:

            print("\nERRO INESPERADO:")
            print(erro)

        # Pergunta ao usuário se deseja continuar
        resposta = input(
            "\nDeseja realizar uma nova análise? (S/N): "
        ).strip().upper()

        if resposta != "S":

            print("\nEncerrando o sistema...")

            break


def main():

    """
    Ponto de entrada do programa.
    """

    print("\n========================================")
    print("SISTEMA DE ANALISE DE GRAOS")
    print("========================================")

    executar_sistema()

    print("\nSistema finalizado com sucesso.")


if __name__ == "__main__":

    main()