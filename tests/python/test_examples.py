"""Tests de `examples/`: cada `run.py` es su propia verificacion (termina en
`assert` contra los numeros documentados en su README.md); este archivo solo
confirma que corren sin errores (exit 0) y, para los ejemplos 01-04 (los que
traen `input.json`), que el CLI (`ventpy.cli.main`) reproduce el mismo numero
clave al invocarse con ese `input.json` — el mismo contrato de
`tests/python/test_cli.py`, pero contra los `input.json` publicados en
`examples/` en vez de payloads inline.

Ejemplo 03 (cobertura con una zona en deficit) tiene `exit code == 2`
INTENCIONAL por el CLI (resultado calculado correctamente pero "no
confiable": ver examples/03-cobertura-levantamiento/README.md) — se asserta
como 2, no como 0.
"""
import json
import subprocess
import sys
from pathlib import Path

import pytest

from ventpy import cli

EXAMPLES_DIR = Path(__file__).resolve().parents[2] / "examples"

_ALL_EXAMPLES = [
    "01-demanda-peru",
    "02-preset-chile",
    "03-cobertura-levantamiento",
    "04-red-ventilador",
    "05-ducto-seleccion",
]


@pytest.mark.parametrize("example", _ALL_EXAMPLES)
def test_run_py_exits_0(example):
    """Corre `examples/<caso>/run.py` con el mismo interprete que corre este
    test (subprocess, no `runpy` en proceso, para no compartir estado global
    de `ventpy` entre ejemplos) y exige exit 0 — el script ES su propia
    verificacion via `assert`."""
    run_py = EXAMPLES_DIR / example / "run.py"
    assert run_py.is_file(), f"falta {run_py}"

    result = subprocess.run(
        [sys.executable, str(run_py)],
        capture_output=True, text=True, cwd=str(run_py.parent),
    )

    assert result.returncode == 0, (
        f"{example}/run.py termino con exit {result.returncode}\n"
        f"--- stdout ---\n{result.stdout}\n"
        f"--- stderr ---\n{result.stderr}"
    )


class TestCliReproduceReadmeNumbers:
    """Para 01-04: invoca `ventpy.cli.main` con el `input.json` publicado en
    `examples/` y verifica el numero clave documentado en el README de cada
    ejemplo (mismo `input.json` que el bloque "Comando CLI" de cada README)."""

    def test_01_demanda_peru(self, capsys):
        archivo = str(EXAMPLES_DIR / "01-demanda-peru" / "input.json")

        exit_code = cli.main(["demanda", archivo, "--json"])

        assert exit_code == 0
        data = json.loads(capsys.readouterr().out)
        assert data["q_total_m3min"] == 981.0
        assert data["governing_factor"] == "diesel (Q_Eq)"
        assert data["velocity_ok"] is True

    def test_02_preset_chile(self, capsys):
        archivo = str(EXAMPLES_DIR / "02-preset-chile" / "input.json")

        exit_code = cli.main(["demanda", archivo, "--norma", "chile", "--json"])

        assert exit_code == 0
        data = json.loads(capsys.readouterr().out)
        assert data["standard"] == "DS132_Chile"
        assert data["q_total_m3min"] == 981.0
        assert data["governing_factor"] == "diesel (Q_Eq)"

    def test_03_cobertura_levantamiento_exit2_intencional(self, capsys):
        archivo = str(EXAMPLES_DIR / "03-cobertura-levantamiento" / "input.json")

        exit_code = cli.main(["cobertura", archivo, "--json"])

        # Exit 2 INTENCIONAL (ver README del ejemplo): global_compliant=True
        # pero la zona "Frente N-02" esta en deficit -> compliant=False.
        assert exit_code == 2
        data = json.loads(capsys.readouterr().out)
        assert data["global_compliant"] is True
        assert data["all_zones_compliant"] is False
        assert data["compliant"] is False
        assert data["zones"][1]["zone_name"] == "Frente N-02"
        assert data["zones"][1]["deficit_m3min"] == 57.0

    def test_04_red_ventilador(self, capsys):
        archivo = str(EXAMPLES_DIR / "04-red-ventilador" / "input.json")

        exit_code = cli.main(["ventilador", archivo, "--json"])

        assert exit_code == 0
        data = json.loads(capsys.readouterr().out)
        assert data["q_m3min"] == pytest.approx(2797.4, abs=1.0)
        assert data["stall_ok"] is True
        assert data["network"]["converged"] is True
