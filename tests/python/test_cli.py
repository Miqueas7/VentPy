import json

import pytest

from ventpy import cli


_DEVFACE_15W_4200_SCOOP150 = {
    "zone_type": "DevelopmentFace",
    "num_workers": 15,
    "altitude_masl": 4200.0,
    "diesel_fleet": [
        {"name": "Scoop ST7", "horsepower": 150, "availability": 0.85, "utilization": 0.70}
    ],
    "blasting_params": {
        "explosive_kg": 50,
        "gas_volume_per_kg": 0.04,
        "dilution_time_min": 30,
        "face_area_m2": 12,
        "face_length_m": 200,
    },
}

_CHILE_10W_4200_SCOOP150 = {
    "num_workers": 10,
    "altitude_masl": 4200.0,
    "diesel_fleet": [
        {"name": "Scoop ST7", "horsepower": 150, "availability": 0.85, "utilization": 0.70}
    ],
}


def _write_json(tmp_path, name, data):
    path = tmp_path / name
    path.write_text(json.dumps(data), encoding="utf-8")
    return str(path)


class TestCmdDemanda:
    def test_demanda_devface_981(self, tmp_path, capsys):
        archivo = _write_json(tmp_path, "input.json", _DEVFACE_15W_4200_SCOOP150)

        exit_code = cli.main(["demanda", archivo, "--json"])

        assert exit_code == 0
        data = json.loads(capsys.readouterr().out)
        assert data["q_total_m3min"] == 981.0
        assert data["governing_factor"] == "diesel (Q_Eq)"

    def test_demanda_norma_chile(self, tmp_path, capsys):
        archivo = _write_json(tmp_path, "input.json", _CHILE_10W_4200_SCOOP150)

        exit_code = cli.main(["demanda", archivo, "--norma", "chile", "--json"])

        assert exit_code == 0
        data = json.loads(capsys.readouterr().out)
        assert data["standard"] == "DS132_Chile"
        assert data["q_total_m3min"] == 981.0
        assert data["governing_factor"] == "diesel (Q_Eq)"

    def test_demanda_clave_desconocida_exit1(self, tmp_path, capsys):
        archivo = _write_json(tmp_path, "input.json", {"trabajadores": 5})

        exit_code = cli.main(["demanda", archivo, "--json"])

        assert exit_code == 1
        stderr = capsys.readouterr().err
        assert "trabajadores" in stderr

    def test_archivo_inexistente_exit1(self, tmp_path, capsys):
        archivo = str(tmp_path / "no_existe.json")

        exit_code = cli.main(["demanda", archivo])

        assert exit_code == 1
        stderr = capsys.readouterr().err
        assert "error:" in stderr
        assert "no_existe.json" in stderr

    def test_modo_texto_advertencias_visibles(self, tmp_path, capsys):
        # 4800 msnm > 4500: dispara la advertencia "ALTITUD EXTREMA" del
        # Governor (governor.hpp::generate_warnings). Caso simple (solo
        # personal) para no acoplar el test a ningun otro factor.
        archivo = _write_json(
            tmp_path, "input.json", {"num_workers": 5, "altitude_masl": 4800.0}
        )

        exit_code = cli.main(["demanda", archivo])

        assert exit_code == 0
        stdout = capsys.readouterr().out
        assert "ALTITUD" in stdout


class TestCmdLmp:
    def test_lmp_chile_co(self, capsys):
        exit_code = cli.main(["lmp", "--norma", "chile", "--gas", "CO", "--json"])

        assert exit_code == 0
        data = json.loads(capsys.readouterr().out)
        assert data["twa_8h"] == 44.0
        assert "40 ppm" in data["regulation_ref"]

    def test_lmp_gas_no_regulado_exit1(self, capsys):
        exit_code = cli.main(["lmp", "--norma", "chile", "--gas", "NO", "--json"])

        assert exit_code == 1
        assert capsys.readouterr().err

    def test_lmp_tabla_completa(self, capsys):
        exit_code = cli.main(["lmp", "--norma", "peru", "--json"])

        assert exit_code == 0
        data = json.loads(capsys.readouterr().out)
        assert len(data["gas_limits"]) == 8


class TestMainMisc:
    def test_help_exits_zero(self, capsys):
        exit_code = cli.main(["--help"])

        assert exit_code == 0

    def test_no_command_exits_nonzero(self, capsys):
        exit_code = cli.main([])

        assert exit_code != 0
