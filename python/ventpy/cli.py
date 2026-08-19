"""CLI de VentPy — capa de presentación sobre el núcleo C++.

Sin lógica de cálculo (regla 6): este módulo solo parsea JSON, construye
structs del API, invoca el núcleo y formatea resultados.
Exit codes: 0 ok · 1 error de entrada · 2 resultado no confiable.
"""
import argparse, json, sys
import ventpy


# ============================================================================
# Whitelists (= campos def_rw expuestos en bindings/bindings.cpp)
# ============================================================================
_NORMAS = {
    "peru": ventpy.RegulatoryStandard.DS024_Peru,
    "chile": ventpy.RegulatoryStandard.DS132_Chile,
}

_ALLOWED_ATMOSPHERIC = frozenset({
    "altitude_masl", "barometric_pressure_kpa", "dry_bulb_temp_c",
    "wet_bulb_temp_c", "relative_humidity",
})

_ALLOWED_DIESEL_EQUIPMENT = frozenset({
    "name", "horsepower", "availability", "utilization", "emission_tier",
})

_ALLOWED_BLASTING = frozenset({
    "explosive_kg", "explosive_type", "gas_volume_per_kg", "dilution_time_min",
    "face_area_m2", "face_length_m", "co_per_kg_liters", "nox_per_kg_liters",
    "target_co_ppm", "target_nox_ppm", "min_velocity_mps",
})

_ALLOWED_DUST = frozenset({
    "dust_generation_rate_mg_s", "silica_content_percent",
    "target_concentration_mg_m3", "face_area_m2", "water_suppression",
    "suppression_efficiency",
})

_ALLOWED_THERMAL = frozenset({
    "virgin_rock_temp_c", "geothermal_gradient_c_per_100m",
    "depth_below_surface_m", "auto_compression_c_per_100m",
    "heat_from_equipment_kw", "heat_from_oxidation_kw",
    "target_effective_temp_c", "face_area_m2",
})

_ALLOWED_INPUT = frozenset({
    "zone_type", "num_workers", "altitude_masl", "face_area_m2",
    "face_length_m", "simultaneity_factor", "leakage_factor",
    "safety_factor", "atmospheric", "diesel_fleet", "blasting_params",
    "dust_params", "thermal_params",
})


# ============================================================================
# Helpers de presentación (regla 6: sin decisiones de física aquí)
# ============================================================================
def _norma(nombre):
    """"peru" | "chile" -> RegulatoryConfig (preset oficial de la norma)."""
    try:
        standard = _NORMAS[nombre]
    except KeyError:
        raise ValueError(
            f"norma desconocida '{nombre}' (validas: {', '.join(sorted(_NORMAS))})"
        )
    return ventpy.RegulatoryConfig.for_standard(standard)


def _enum(cls, nombre, campo):
    """string -> miembro de enum `cls`; ValueError con opciones validas."""
    try:
        return cls.__members__[nombre]
    except KeyError:
        opciones = ", ".join(sorted(cls.__members__))
        raise ValueError(
            f"valor invalido '{nombre}' para '{campo}' (validas: {opciones})"
        )


def _build(struct_cls, data, allowed, builders=None):
    """Instancia struct_cls() y aplica cada clave de `data`.

    Clave fuera de `allowed` -> ValueError. Si hay un builder registrado
    para la clave (construccion de sub-struct/enum anidado) se usa antes
    de asignar; si no, setattr directo.
    """
    builders = builders or {}
    obj = struct_cls()
    for key, value in data.items():
        if key not in allowed:
            raise ValueError(
                f"clave desconocida '{key}' en {struct_cls.__name__} "
                f"(validas: {', '.join(sorted(allowed))})"
            )
        if key in builders:
            value = builders[key](value)
        setattr(obj, key, value)
    return obj


def _emit(obj_dict, as_json):
    """Imprime `obj_dict` (dict) como JSON o como reporte legible simple."""
    if as_json:
        print(json.dumps(obj_dict, indent=2, ensure_ascii=False))
        return
    _print_report(obj_dict, indent=0)


def _print_report(value, indent):
    pad = "  " * indent
    if isinstance(value, dict):
        for key, sub in value.items():
            if isinstance(sub, (dict, list)) and sub:
                print(f"{pad}{key}:")
                _print_report(sub, indent + 1)
            else:
                print(f"{pad}{key}: {sub}")
    elif isinstance(value, list):
        for i, item in enumerate(value):
            print(f"{pad}- [{i}]")
            _print_report(item, indent + 1)
    else:
        print(f"{pad}{value}")


def _flags_exit(ok_flags):
    """dict de banderas (nombre -> bool) -> exit 0 si todas ok, si no 2."""
    return 0 if all(ok_flags.values()) else 2


# ============================================================================
# Construccion de VentilationInput desde JSON
# ============================================================================
def _build_diesel_fleet(items):
    fleet = ventpy.DieselFleet()
    for item in items:
        equip = _build(
            ventpy.DieselEquipment, item, _ALLOWED_DIESEL_EQUIPMENT,
            builders={
                "emission_tier": lambda v: _enum(
                    ventpy.EngineEmissionTier, v, "emission_tier"),
            },
        )
        fleet.add_equipment(equip)
    return fleet


def _build_ventilation_input(data):
    builders = {
        "zone_type": lambda v: _enum(ventpy.ZoneType, v, "zone_type"),
        "atmospheric": lambda v: _build(
            ventpy.AtmosphericParams, v, _ALLOWED_ATMOSPHERIC),
        "diesel_fleet": _build_diesel_fleet,
        "blasting_params": lambda v: _build(
            ventpy.BlastingParams, v, _ALLOWED_BLASTING,
            builders={
                "explosive_type": lambda x: _enum(
                    ventpy.ExplosiveType, x, "explosive_type"),
            },
        ),
        "dust_params": lambda v: _build(ventpy.DustParams, v, _ALLOWED_DUST),
        "thermal_params": lambda v: _build(
            ventpy.ThermalParams, v, _ALLOWED_THERMAL),
    }
    return _build(ventpy.VentilationInput, data, _ALLOWED_INPUT, builders=builders)


def _demanda_report(result):
    return {
        "standard": result.standard.name,
        "zone_type": result.zone_type.name,
        "q_personnel_m3min": result.q_personnel_m3min,
        "q_diesel_m3min": result.q_diesel_m3min,
        "q_blasting_m3min": result.q_blasting_m3min,
        "q_dust_m3min": result.q_dust_m3min,
        "q_thermal_m3min": result.q_thermal_m3min,
        "q_leakage_m3min": result.q_leakage_m3min,
        "governing_factor": result.governing_factor,
        "q_governing_m3min": result.q_governing_m3min,
        "q_at_fan_m3min": result.q_at_fan_m3min,
        "q_total_m3min": result.q_total_m3min,
        "q_total_m3s": result.q_total_m3s,
        "q_total_cfm": result.q_total_cfm,
        "face_area_m2": result.face_area_m2,
        "velocity_at_face_mps": result.velocity_at_face_mps,
        "velocity_ok": result.velocity_ok,
        "safety_factor_applied": result.safety_factor_applied,
        "warnings": list(result.warnings),
        "notes": result.notes,
    }


def _gas_limit_report(limit):
    return {
        "gas": limit.gas.name,
        "unit": limit.unit.name,
        "twa_8h": limit.twa_8h,
        "stel": limit.stel,
        "ceiling": limit.ceiling,
        "floor_min": limit.floor_min,
        "regulation_ref": limit.regulation_ref,
    }


# ============================================================================
# Subcomandos
# ============================================================================
def cmd_demanda(args):
    with open(args.archivo, "r", encoding="utf-8") as f:
        data = json.load(f)

    vent_input = _build_ventilation_input(data)
    config = _norma(args.norma)
    governor = ventpy.VentilationGovernor(config)
    result = governor.calculate_total_demand(vent_input)

    _emit(_demanda_report(result), args.json)
    # T1: la demanda siempre reporta exit 0 (salvo excepcion, capturada en
    # main). Los criterios "no confiable" (velocity_ok, NO CONVERGIO) rigen
    # los subcomandos de T2 (cobertura/red/ventilador) via _flags_exit.
    return 0


def cmd_lmp(args):
    config = _norma(args.norma)
    standard = config.standard

    if args.gas:
        gas = _enum(ventpy.GasType, args.gas, "gas")
        limit = ventpy.lmp_for(standard, gas)
        report = {"standard": standard.name, **_gas_limit_report(limit)}
    else:
        limits = ventpy.gas_limits(standard)
        report = {
            "standard": standard.name,
            "gas_limits": [_gas_limit_report(l) for l in limits],
        }

    _emit(report, args.json)
    return 0


# ============================================================================
# Parser / entrypoint
# ============================================================================
def build_parser():
    parser = argparse.ArgumentParser(
        prog="ventpy",
        description="VentPy - calculos de ventilacion de minas subterraneas "
                     "(DS 024-2016-EM Peru / DS 132 Chile).",
    )
    subparsers = parser.add_subparsers(dest="command", required=True)

    demanda = subparsers.add_parser(
        "demanda", help="Demanda total de ventilacion de una zona/frente")
    demanda.add_argument("archivo", help="JSON con VentilationInput")
    demanda.add_argument(
        "--norma", choices=sorted(_NORMAS), default="peru",
        help="Norma regulatoria a aplicar (default: peru)")
    demanda.add_argument(
        "--json", action="store_true", help="Salida en formato JSON")
    demanda.set_defaults(func=cmd_demanda)

    lmp = subparsers.add_parser(
        "lmp", help="Limites maximos permisibles (LMP) de gases regulados")
    lmp.add_argument(
        "--norma", choices=sorted(_NORMAS), default="peru",
        help="Norma regulatoria a aplicar (default: peru)")
    lmp.add_argument(
        "--gas", default=None,
        help="Gas especifico (ej. CO); si se omite, imprime la tabla completa")
    lmp.add_argument(
        "--json", action="store_true", help="Salida en formato JSON")
    lmp.set_defaults(func=cmd_lmp)

    # T2: cmd_cobertura, cmd_red, cmd_ventilador

    return parser


def main(argv=None):
    parser = build_parser()
    try:
        args = parser.parse_args(argv)
    except SystemExit as exc:
        code = exc.code
        if code is None:
            return 0
        if isinstance(code, int):
            return code
        return 1

    try:
        return args.func(args)
    except (ValueError, OSError) as exc:
        # ValueError: whitelist/enum invalidos o excepciones del nucleo C++
        # (nanobind traduce std::invalid_argument -> ValueError).
        # OSError: archivo de entrada inexistente/sin permisos/ruta invalida
        # (FileNotFoundError, PermissionError, etc. heredan de OSError).
        print(f"error: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
