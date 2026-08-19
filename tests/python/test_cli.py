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


class TestCmdCobertura:
    def test_cobertura_deficit_zona(self, tmp_path, capsys):
        # Espejo de TestAnalyzeSurvey.test_global_covers_with_zone_in_deficit
        # (tests/python/test_cobertura.py): 2 zonas DevelopmentFace/10 trab.,
        # 4200msnm->270 medido (207 requerido, OK) y 2500msnm->150 medido
        # (207 requerido, deficit 57). Global cubre (420>=414) pero la zona 2
        # esta en deficit -> compliant estricto = False -> exit 2.
        payload = {
            "zones": [
                {
                    "zone_name": "Rampa 4200",
                    "input": {
                        "zone_type": "DevelopmentFace",
                        "num_workers": 10,
                        "altitude_masl": 4200.0,
                    },
                    "measurement": {"q_measured_m3min": 270.0},
                },
                {
                    "zone_name": "Frente N-02",
                    "input": {
                        "zone_type": "DevelopmentFace",
                        "num_workers": 10,
                        "altitude_masl": 2500.0,
                    },
                    "measurement": {"q_measured_m3min": 150.0},
                },
            ],
        }
        archivo = _write_json(tmp_path, "cobertura.json", payload)

        exit_code = cli.main(["cobertura", archivo, "--json"])

        assert exit_code == 2
        data = json.loads(capsys.readouterr().out)
        assert data["global_compliant"] is True
        assert data["all_zones_compliant"] is False
        assert data["compliant"] is False
        assert data["zones"][1]["deficit_m3min"] == 57.0

    def test_cobertura_clave_desconocida_exit1(self, tmp_path, capsys):
        archivo = _write_json(tmp_path, "cobertura.json", {"zonas": []})

        exit_code = cli.main(["cobertura", archivo, "--json"])

        assert exit_code == 1
        stderr = capsys.readouterr().err
        assert "zonas" in stderr

    def test_cobertura_modo_texto_tabla(self, tmp_path, capsys):
        # Fix round (hallazgo 2): modo texto debe ser tabla, no el dump
        # generico. La cabecera lleva las columnas del brief y la zona en
        # deficit debe aparecer marcada como tal.
        payload = {
            "zones": [
                {
                    "zone_name": "Rampa 4200",
                    "input": {
                        "zone_type": "DevelopmentFace",
                        "num_workers": 10,
                        "altitude_masl": 4200.0,
                    },
                    "measurement": {"q_measured_m3min": 270.0},
                },
                {
                    "zone_name": "Frente N-02",
                    "input": {
                        "zone_type": "DevelopmentFace",
                        "num_workers": 10,
                        "altitude_masl": 2500.0,
                    },
                    "measurement": {"q_measured_m3min": 150.0},
                },
            ],
        }
        archivo = _write_json(tmp_path, "cobertura.json", payload)

        exit_code = cli.main(["cobertura", archivo])

        assert exit_code == 2
        stdout = capsys.readouterr().out
        assert "ZONA" in stdout
        assert "REQUERIDO" in stdout
        assert "MEDIDO" in stdout
        assert "COBERTURA" in stdout
        assert "ESTADO" in stdout
        assert "DEFICIT" in stdout


_RED_A_BRANCHES = [
    {"branch_id": "F", "from_node": "S", "to_node": "A", "r_manual": 0.05,
     "fan_pressure_pa": 500.0},
    {"branch_id": "P1", "from_node": "A", "to_node": "B", "r_manual": 0.2},
    {"branch_id": "P2", "from_node": "A", "to_node": "B", "r_manual": 0.8},
    {"branch_id": "R", "from_node": "B", "to_node": "S", "r_manual": 0.1},
]

_RED_A_SIN_FAN_BRANCHES = [
    {"branch_id": "F", "from_node": "S", "to_node": "A", "r_manual": 0.05},
    {"branch_id": "P1", "from_node": "A", "to_node": "B", "r_manual": 0.2},
    {"branch_id": "P2", "from_node": "A", "to_node": "B", "r_manual": 0.8},
    {"branch_id": "R", "from_node": "B", "to_node": "S", "r_manual": 0.1},
]


class TestCmdRed:
    def test_red_dos_mallas(self, tmp_path, capsys):
        # Espejo de TestNetworkSolver.test_red_paralela_converge_a_la_solucion_analitica
        # (tests/python/test_red_ventilacion.py): red A, Q de F ~ 2744.974.
        payload = {
            "branches": _RED_A_BRANCHES,
            "solver": {"tolerance_m3min": 0.006, "max_iterations": 1000},
        }
        archivo = _write_json(tmp_path, "red.json", payload)

        exit_code = cli.main(["red", archivo, "--json"])

        assert exit_code == 0
        data = json.loads(capsys.readouterr().out)
        assert data["converged"] is True
        f = data["branches"][0]
        assert f["branch_id"] == "F"
        assert f["q_m3min"] == pytest.approx(2744.974, abs=0.05)

    def test_red_no_converge_exit2(self, tmp_path, capsys):
        payload = {
            "branches": _RED_A_BRANCHES,
            "solver": {"tolerance_m3min": 1e-9, "max_iterations": 1},
        }
        archivo = _write_json(tmp_path, "red.json", payload)

        exit_code = cli.main(["red", archivo, "--json"])

        assert exit_code == 2
        data = json.loads(capsys.readouterr().out)
        assert data["converged"] is False

    def test_red_modo_texto_tabla(self, tmp_path, capsys):
        # Fix round (hallazgo 2): tabla ramal / Q / dP + pie de convergencia.
        payload = {
            "branches": _RED_A_BRANCHES,
            "solver": {"tolerance_m3min": 0.006, "max_iterations": 1000},
        }
        archivo = _write_json(tmp_path, "red.json", payload)

        exit_code = cli.main(["red", archivo])

        assert exit_code == 0
        stdout = capsys.readouterr().out
        assert "RAMAL" in stdout
        assert "Q [m3/min]" in stdout
        assert "dP [Pa]" in stdout
        assert "F" in stdout


class TestCmdVentilador:
    def test_ventilador_simple(self, tmp_path, capsys):
        # Espejo de TestFanOperatingPoint.test_interseccion_analitica:
        # curva lineal (600,3000)-(3000,600), r=0.5 -> q ~ 2637.29.
        payload = {
            "curve": {
                "fan_id": "LIN",
                "points": [[600.0, 3000.0], [3000.0, 600.0]],
            },
            "mode": "simple",
            "r_system_ns2m8": 0.5,
        }
        archivo = _write_json(tmp_path, "ventilador.json", payload)

        exit_code = cli.main(["ventilador", archivo, "--json"])

        assert exit_code == 0
        data = json.loads(capsys.readouterr().out)
        assert data["q_m3min"] == pytest.approx(2637.29, abs=0.01)
        assert data["stall_ok"] is True

    def test_ventilador_en_red(self, tmp_path, capsys):
        # Espejo de TestFanOperatingPointInNetwork.test_punto_fijo_converge_al_equilibrio:
        # red A sin fan + curva AX-RED en el ramal F -> q ~ 2797.44.
        payload = {
            "curve": {
                "fan_id": "AX-RED",
                "points": [
                    [1200.0, 900.0], [1800.0, 800.0], [2400.0, 650.0],
                    [3000.0, 450.0], [3600.0, 200.0],
                ],
            },
            "mode": "red",
            "fan_branch_id": "F",
            "network": {
                "branches": _RED_A_SIN_FAN_BRANCHES,
                "solver": {"tolerance_m3min": 0.006, "max_iterations": 1000},
            },
        }
        archivo = _write_json(tmp_path, "ventilador.json", payload)

        exit_code = cli.main(["ventilador", archivo, "--json"])

        assert exit_code == 0
        data = json.loads(capsys.readouterr().out)
        assert data["q_m3min"] == pytest.approx(2797.44, abs=1.0)
        assert data["network"]["converged"] is True

    def test_ventilador_stall_exit2(self, tmp_path, capsys):
        # Espejo de TestFanOperatingPoint.test_zona_de_stall_detectada:
        # curva con pico, r=5.722049850540529 -> stall_ok False.
        payload = {
            "curve": {
                "fan_id": "PICO",
                "points": [
                    [600.0, 1500.0], [1200.0, 2000.0], [1800.0, 1900.0],
                    [2400.0, 1200.0], [3000.0, 400.0],
                ],
            },
            "mode": "simple",
            "r_system_ns2m8": 5.722049850540529,
        }
        archivo = _write_json(tmp_path, "ventilador.json", payload)

        exit_code = cli.main(["ventilador", archivo, "--json"])

        assert exit_code == 2
        data = json.loads(capsys.readouterr().out)
        assert data["stall_ok"] is False

    def test_ventilador_red_atmospheric_toplevel_exit1(self, tmp_path, capsys):
        # Fix round (hallazgo 1): en modo "red" la atmosfera vive DENTRO de
        # network{}; un atmospheric de nivel superior se descartaria en
        # silencio (el usuario obtendria un punto de operacion con la
        # atmosfera equivocada sin ninguna senal). Debe rechazarse.
        payload = {
            "curve": {
                "fan_id": "AX-RED",
                "points": [
                    [1200.0, 900.0], [1800.0, 800.0], [2400.0, 650.0],
                    [3000.0, 450.0], [3600.0, 200.0],
                ],
            },
            "atmospheric": {"altitude_masl": 4200.0},
            "mode": "red",
            "fan_branch_id": "F",
            "network": {
                "branches": _RED_A_SIN_FAN_BRANCHES,
                "solver": {"tolerance_m3min": 0.006, "max_iterations": 1000},
            },
        }
        archivo = _write_json(tmp_path, "ventilador.json", payload)

        exit_code = cli.main(["ventilador", archivo, "--json"])

        assert exit_code == 1
        stderr = capsys.readouterr().err
        assert "network" in stderr

    def test_ventilador_modo_texto_bloques(self, tmp_path, capsys):
        # Fix round (hallazgo 2): bloque Q/P de operacion, pico y margen de
        # stall con veredicto, en vez del dump generico.
        payload = {
            "curve": {
                "fan_id": "LIN",
                "points": [[600.0, 3000.0], [3000.0, 600.0]],
            },
            "mode": "simple",
            "r_system_ns2m8": 0.5,
        }
        archivo = _write_json(tmp_path, "ventilador.json", payload)

        exit_code = cli.main(["ventilador", archivo])

        assert exit_code == 0
        stdout = capsys.readouterr().out
        assert "PUNTO DE OPERACION" in stdout
        assert "PICO" in stdout
        assert "STALL" in stdout.upper()


class TestMainMisc:
    def test_help_exits_zero(self, capsys):
        exit_code = cli.main(["--help"])

        assert exit_code == 0

    def test_no_command_exits_nonzero(self, capsys):
        exit_code = cli.main([])

        assert exit_code != 0
