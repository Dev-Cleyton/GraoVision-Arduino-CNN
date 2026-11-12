"""Entrada principal GraoVision: abre a interface grafica por padrao.

Use: python main.py --terminal para programa de console legado.
"""
import sys


def main():
    if len(sys.argv) > 1 and sys.argv[1] in ('--terminal', '--console'):
        from main_terminal import main as terminal_main
        terminal_main()
    else:
        print('GRAOVISION: abrindo interface grafica. Modo terminal: python main.py --terminal')
        from iniciar_interface import run
        run()


if __name__ == '__main__':
    main()
