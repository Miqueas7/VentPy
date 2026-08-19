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

# --- T2: cobertura ----------------------------------------------------------
_ALLOWED_AIRFLOW_STATION = frozenset({"station_id", "area_m2", "velocity_mps"})

_ALLOWED_ZONE_MEASUREMENT = frozenset({
    "zone_name", "q_measured_m3min", "stations",
})

_ALLOWED_ZONE_SURVEY = frozenset({"zone_name", "input", "measurement"})

_ALLOWED_COVERAGE_PARAMS = frozenset({
    "warning_margin", "overventilation_factor", "min_velocity_mpm",
    "max_velocity_mpm", "anfo_or_blasting_agents",
})

_ALLOWED_COBERTURA_REQUEST = frozenset({"zones", "params"})

# --- T2: red -----------------------------------------------------------------
_ALLOWED_SINGULARITY = frozenset({
    "type", "shock_factor_x", "area_ratio", "description",
})

_ALLOWED_AIRWAY = frozenset({
    "airway_id", "length_m", "perimeter_m", "area_m2", "lining",
    "atkinson_k", "singularities",
})

_ALLOWED_BRANCH = frozenset({
    "branch_id", "from_node", "to_node", "airway", "r_manual",
    "fan_pressure_pa", "q_initial_m3min",
})

_ALLOWED_SOLVER_PARAMS = frozenset({"tolerance_m3min", "max_iterations"})

_ALLOWED_RED_REQUEST = frozenset({"branches", "solver", "atmospheric"})

# --- T2: ventilador ------------------------------------------------------
_ALLOWED_FAN_CURVE = frozenset({"fan_id", "rated_density_kg_m3", "points"})

_ALLOWED_FAN_OPERATING_PARAMS = frozenset({
    "stall_margin", "under_relaxation", "max_iterations",
})

_ALLOWED_VENTILADOR_REQUEST = frozenset({
    "curve", "atmospheric", "params", "mode", "r_system_ns2m8",
    "network", "fan_branch_id",
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

    Revision final SP-5 (hallazgo F-1): `data` puede venir con forma
    equivocada desde JSON de usuario (struct esperado pero llega una lista,
    tipo escalar equivocado para un campo). Sin estas dos guardas, nanobind
    deja pasar un TypeError/AttributeError crudo hasta consola (traceback
    completo) en vez del ValueError limpio que `main()` sabe presentar.
    """
    builders = builders or {}
    if not isinstance(data, dict):
        raise ValueError(
            f"se esperaba un objeto (dict) para {struct_cls.__name__}, "
            f"se recibio {type(data).__name__}"
        )
    obj = struct_cls()
    for key, value in data.items():
        if key not in allowed:
            raise ValueError(
                f"clave desconocida '{key}' en {struct_cls.__name__} "
                f"(validas: {', '.join(sorted(allowed))})"
            )
        if key in builders:
            try:
                value = builders[key](value)
            except ValueError as e:
                # Propaga el error del sub-struct/enum anidado con la clave
                # contenedora al frente (ej. "blasting_params": ... en vez
                # de solo "BlastingParams": ...) para que el usuario ubique
                # de inmediato que parte del JSON esta mal formada.
                raise ValueError(f"'{key}': {e}") from e
        try:
            setattr(obj, key, value)
        except TypeError as e:
            raise ValueError(
                f"valor invalido para '{key}' en {struct_cls.__name__}: {e}"
            ) from e
    return obj


def _check_allowed(data, allowed, label):
    """Como `_build` pero para dicts 'request' de T2 (no struct_cls unico:
    agrupan varios structs/listas). Mismo formato de error que `_build`."""
    for key in data:
        if key not in allowed:
            raise ValueError(
                f"clave desconocida '{key}' en {label} "
                f"(validas: {', '.join(sorted(allowed))})"
            )


def _emit(obj_dict, as_json, text_printer=None):
    """Imprime `obj_dict` (dict) como JSON o como reporte legible.

    `--json` SIEMPRE es el dump generico de `obj_dict` (contrato estable).
    En modo texto, si el subcomando registro un `text_printer` (tabla a
    medida) se usa ese; si no, cae al dump generico indentado.
    """
    if as_json:
        print(json.dumps(obj_dict, indent=2, ensure_ascii=False))
        return
    if text_printer is not None:
        text_printer(obj_dict)
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
# T2: Construccion de cobertura desde JSON
# ============================================================================
def _build_zone_measurement(data):
    return _build(
        ventpy.ZoneMeasurement, data, _ALLOWED_ZONE_MEASUREMENT,
        builders={
            "stations": lambda v: [
                _build(ventpy.AirflowStation, s, _ALLOWED_AIRFLOW_STATION)
                for s in v
            ],
        },
    )


def _build_zone_survey(data):
    return _build(
        ventpy.ZoneSurvey, data, _ALLOWED_ZONE_SURVEY,
        builders={
            "input": _build_ventilation_input,
            "measurement": _build_zone_measurement,
        },
    )


def _build_coverage_params(data):
    return _build(ventpy.CoverageParams, data, _ALLOWED_COVERAGE_PARAMS)


def _station_result_report(station):
    return {
        "station_id": station.station_id,
        "area_m2": station.area_m2,
        "velocity_mps": station.velocity_mps,
        "velocity_mpm": station.velocity_mpm,
        "q_station_m3min": station.q_station_m3min,
        "velocity_ok": station.velocity_ok,
        "warning": station.warning,
    }


def _coverage_status(zone):
    """Etiqueta de presentacion (OK/DEFICIT/...) derivada de las banderas ya
    calculadas por el nucleo - mapeo puro, ninguna decision de fisica."""
    if not zone.compliant:
        return "DEFICIT"
    if zone.overventilated:
        return "SOBREVENTILADO"
    if zone.near_deficit_warning:
        return "ADVERTENCIA"
    return "OK"


def _zone_coverage_report(zone):
    return {
        "zone_name": zone.zone_name,
        "status": _coverage_status(zone),
        "q_required_m3min": zone.q_required_m3min,
        "q_measured_m3min": zone.q_measured_m3min,
        "coverage_ratio": zone.coverage_ratio,
        "coverage_percent": zone.coverage_ratio * 100.0,
        "deficit_m3min": zone.deficit_m3min,
        "compliant": zone.compliant,
        "near_deficit_warning": zone.near_deficit_warning,
        "overventilated": zone.overventilated,
        "stations": [_station_result_report(s) for s in zone.stations],
        "regulation_ref": zone.regulation_ref,
    }


def _mine_coverage_report(result):
    return {
        "global_compliant": result.global_compliant,
        "all_zones_compliant": result.all_zones_compliant,
        "compliant": result.compliant,
        "q_required_total_m3min": result.q_required_total_m3min,
        "q_measured_total_m3min": result.q_measured_total_m3min,
        "coverage_ratio": result.coverage_ratio,
        "deficit_total_m3min": result.deficit_total_m3min,
        "warnings": list(result.warnings),
        "regulation_ref": result.regulation_ref,
        "zones": [_zone_coverage_report(z) for z in result.zones],
    }


# Traduccion SOLO para la columna ESTADO de la tabla de texto (el campo
# "status" del --json NO cambia: sigue siendo OK/DEFICIT/SOBREVENTILADO/
# ADVERTENCIA, contrato estable de _coverage_status).
_COVERAGE_STATUS_LABEL = {
    "OK": "OK",
    "DEFICIT": "DEFICIT",
    "SOBREVENTILADO": "SOBRE-VENT",
    "ADVERTENCIA": "AJUSTADO",
}


def _print_cobertura_table(report):
    """Reporte tipo tabla (modo texto): zona | requerido | medido |
    cobertura % | estado + totales + advertencias."""
    header = (f"{'ZONA':<24} {'REQUERIDO':>12} {'MEDIDO':>12} "
              f"{'COBERTURA %':>12}  {'ESTADO':<12}")
    print(header)
    print("-" * len(header))
    for zone in report["zones"]:
        estado = _COVERAGE_STATUS_LABEL.get(zone["status"], zone["status"])
        print(
            f"{zone['zone_name']:<24} {zone['q_required_m3min']:>12.2f} "
            f"{zone['q_measured_m3min']:>12.2f} "
            f"{zone['coverage_percent']:>11.1f}%  {estado:<12}"
        )
    print()
    print(f"TOTAL REQUERIDO : {report['q_required_total_m3min']:.2f} m3/min")
    print(f"TOTAL MEDIDO    : {report['q_measured_total_m3min']:.2f} m3/min")
    print(f"COBERTURA GLOBAL: {report['coverage_ratio'] * 100.0:.1f} %")
    print(f"DEFICIT TOTAL   : {report['deficit_total_m3min']:.2f} m3/min")
    print(
        f"global_compliant={report['global_compliant']}  "
        f"all_zones_compliant={report['all_zones_compliant']}  "
        f"compliant={report['compliant']}"
    )
    if report["warnings"]:
        print("ADVERTENCIAS:")
        for w in report["warnings"]:
            print(f"  - {w}")


# ============================================================================
# T2: Construccion de red (NetworkSolver) desde JSON
# ============================================================================
def _build_singularity(data):
    return _build(
        ventpy.AirwaySingularity, data, _ALLOWED_SINGULARITY,
        builders={
            "type": lambda v: _enum(ventpy.SingularityType, v, "type"),
        },
    )


def _build_airway(data):
    return _build(
        ventpy.AirwayParams, data, _ALLOWED_AIRWAY,
        builders={
            "lining": lambda v: _enum(ventpy.AirwayLining, v, "lining"),
            "singularities": lambda v: [_build_singularity(s) for s in v],
        },
    )


def _build_branch(data):
    return _build(
        ventpy.NetworkBranch, data, _ALLOWED_BRANCH,
        builders={"airway": _build_airway},
    )


def _build_network(branches_data):
    network = ventpy.NetworkDefinition()
    network.branches = [_build_branch(b) for b in branches_data]
    return network


def _build_solver_params(data):
    return _build(ventpy.SolverParams, data, _ALLOWED_SOLVER_PARAMS)


def _build_atmospheric(data):
    return _build(ventpy.AtmosphericParams, data, _ALLOWED_ATMOSPHERIC)


def _build_red_request(data):
    """{branches, solver, atmospheric} -> (NetworkDefinition, atm, SolverParams).

    Reutilizado tal cual por `cmd_red` y por `cmd_ventilador` (modo "red",
    donde `network` sigue exactamente este mismo esquema)."""
    _check_allowed(data, _ALLOWED_RED_REQUEST, "red")
    network = _build_network(data.get("branches", []))
    atm = (_build_atmospheric(data["atmospheric"])
           if "atmospheric" in data else ventpy.AtmosphericParams())
    solver_params = (_build_solver_params(data["solver"])
                      if "solver" in data else ventpy.SolverParams())
    return network, atm, solver_params


def _branch_flow_report(branch):
    return {
        "branch_id": branch.branch_id,
        "from_node": branch.from_node,
        "to_node": branch.to_node,
        "r_ns2m8": branch.r_ns2m8,
        "q_m3min": branch.q_m3min,
        "pressure_drop_pa": branch.pressure_drop_pa,
        "fan_pressure_pa": branch.fan_pressure_pa,
        "velocity_mps": branch.velocity_mps,
        "warnings": list(branch.warnings),
    }


def _red_report(result):
    return {
        "converged": result.converged,
        "iterations": result.iterations,
        "max_residual_m3min": result.max_residual_m3min,
        "mesh_count": result.mesh_count,
        "node_count": result.node_count,
        "warnings": list(result.warnings),
        "biblio_ref": result.biblio_ref,
        "branches": [_branch_flow_report(b) for b in result.branches],
    }


def _print_red_table(report):
    """Reporte tipo tabla (modo texto): ramal | Q | dP + convergencia,
    iteraciones, residual."""
    header = (f"{'RAMAL':<10} {'DESDE->HASTA':<18} "
              f"{'Q [m3/min]':>14} {'dP [Pa]':>14}")
    print(header)
    print("-" * len(header))
    for branch in report["branches"]:
        desde_hasta = f"{branch['from_node']}->{branch['to_node']}"
        print(
            f"{branch['branch_id']:<10} {desde_hasta:<18} "
            f"{branch['q_m3min']:>14.3f} {branch['pressure_drop_pa']:>14.3f}"
        )
    print()
    print(
        f"convergencia={report['converged']}  "
        f"iteraciones={report['iterations']}  "
        f"residual_max={report['max_residual_m3min']:.6f} m3/min"
    )
    print(f"mallas={report['mesh_count']}  nodos={report['node_count']}")
    if report["warnings"]:
        print("ADVERTENCIAS:")
        for w in report["warnings"]:
            print(f"  - {w}")


# ============================================================================
# T2: Construccion de ventilador (FanCalculator) desde JSON
# ============================================================================
def _build_fan_curve_points(points):
    pts = []
    for i, pair in enumerate(points):
        if len(pair) != 2:
            raise ValueError(
                f"punto de curva invalido en indice {i} "
                "(se esperan 2 valores [q_m3min, pressure_pa])"
            )
        point = ventpy.FanCurvePoint()
        point.q_m3min = pair[0]
        point.pressure_pa = pair[1]
        pts.append(point)
    return pts


def _build_fan_curve(data):
    return _build(
        ventpy.FanCurve, data, _ALLOWED_FAN_CURVE,
        builders={"points": _build_fan_curve_points},
    )


def _build_fan_operating_params(data):
    return _build(
        ventpy.FanOperatingParams, data, _ALLOWED_FAN_OPERATING_PARAMS)


def _fan_operating_report(result):
    report = {
        "fan_id": result.fan_id,
        "q_m3min": result.q_m3min,
        "pressure_pa": result.pressure_pa,
        "air_density_kg_m3": result.air_density_kg_m3,
        "density_factor": result.density_factor,
        "in_curve_range": result.in_curve_range,
        "q_peak_m3min": result.q_peak_m3min,
        "pressure_peak_pa": result.pressure_peak_pa,
        "stall_ok": result.stall_ok,
        "stall_margin_actual": result.stall_margin_actual,
        "converged": result.converged,
        "iterations": result.iterations,
        "warnings": list(result.warnings),
        "biblio_ref": result.biblio_ref,
    }
    if result.network is not None:
        report["network"] = _red_report(result.network)
    return report


def _print_ventilador_table(report, stall_margin_required):
    """Reporte de bloques (modo texto): Q/P de operacion, pico, margen de
    stall real vs requerido con veredicto, banderas y advertencias.

    `stall_margin_required` viene de FanOperatingParams.stall_margin (el
    umbral pedido, disponible en cmd_ventilador via el objeto `params`) y
    NO forma parte del dict de `--json` (contrato estable de
    `_fan_operating_report`) - se pasa aparte solo para el texto.
    """
    print(f"VENTILADOR: {report['fan_id']}")
    print(
        f"PUNTO DE OPERACION: Q = {report['q_m3min']:.2f} m3/min  "
        f"P = {report['pressure_pa']:.2f} Pa"
    )
    print(
        f"PICO DE CATALOGO:   Q_pico = {report['q_peak_m3min']:.2f} m3/min  "
        f"P_pico = {report['pressure_peak_pa']:.2f} Pa"
    )
    veredicto = "OK (fuera de la zona de stall)" if report["stall_ok"] \
        else "RIESGO DE STALL (margen insuficiente)"
    print(
        f"MARGEN DE STALL: real = {report['stall_margin_actual'] * 100.0:.1f} %  "
        f"requerido >= {stall_margin_required * 100.0:.1f} %  -> {veredicto}"
    )
    print(
        f"converged={report['converged']}  "
        f"in_curve_range={report['in_curve_range']}  "
        f"stall_ok={report['stall_ok']}"
    )
    if report["warnings"]:
        print("ADVERTENCIAS:")
        for w in report["warnings"]:
            print(f"  - {w}")
    if "network" in report:
        print()
        print("RED ASOCIADA:")
        _print_red_table(report["network"])


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


def cmd_cobertura(args):
    with open(args.archivo, "r", encoding="utf-8") as f:
        data = json.load(f)

    _check_allowed(data, _ALLOWED_COBERTURA_REQUEST, "cobertura")
    zones = [_build_zone_survey(z) for z in data.get("zones", [])]
    params = (_build_coverage_params(data["params"])
              if "params" in data else ventpy.CoverageParams())
    config = _norma(args.norma)

    result = ventpy.CoverageCalculator.analyze_survey(zones, config, params)

    _emit(_mine_coverage_report(result), args.json,
          text_printer=_print_cobertura_table)
    return _flags_exit({"compliant": result.compliant})


def cmd_red(args):
    with open(args.archivo, "r", encoding="utf-8") as f:
        data = json.load(f)

    network, atm, solver_params = _build_red_request(data)
    result = ventpy.NetworkSolver.solve(network, atm, solver_params)

    _emit(_red_report(result), args.json, text_printer=_print_red_table)
    return _flags_exit({"converged": result.converged})


def cmd_ventilador(args):
    with open(args.archivo, "r", encoding="utf-8") as f:
        data = json.load(f)

    _check_allowed(data, _ALLOWED_VENTILADOR_REQUEST, "ventilador")
    curve = (_build_fan_curve(data["curve"])
             if "curve" in data else ventpy.FanCurve())
    params = (_build_fan_operating_params(data["params"])
              if "params" in data else ventpy.FanOperatingParams())
    mode = data.get("mode", "simple")

    if mode == "simple":
        if "r_system_ns2m8" not in data:
            raise ValueError("modo 'simple' requiere 'r_system_ns2m8'")
        # HALLAZGO F-2 (revision final SP-5, gemelo de HALLAZGO 1): "network"
        # y "fan_branch_id" son exclusivos del modo "red". Aceptarlos en
        # modo "simple" los descartaria en silencio (exit 0) dejando creer
        # al usuario que la red que escribio se tuvo en cuenta. Se rechaza
        # explicitamente, igual que el atmospheric sobrante en modo "red".
        if "network" in data or "fan_branch_id" in data:
            sobra = "network" if "network" in data else "fan_branch_id"
            raise ValueError(f"en modo 'simple' sobra '{sobra}'")
        atm = (_build_atmospheric(data["atmospheric"])
               if "atmospheric" in data else ventpy.AtmosphericParams())
        result = ventpy.FanCalculator.operating_point(
            curve, data["r_system_ns2m8"], atm, params)
    elif mode == "red":
        if "atmospheric" in data:
            # HALLAZGO 1 (fix round): en modo "red" la atmosfera vive DENTRO
            # de network{} (misma atm para el solver de red y para la curva
            # del ventilador - operating_point_in_network solo acepta UNA).
            # Aceptar tambien un atmospheric de nivel superior lo
            # descartaria en silencio: el usuario obtendria un punto de
            # operacion calculado con la atmosfera equivocada sin ninguna
            # senal. Se rechaza explicitamente.
            raise ValueError(
                "en modo 'red' la atmosfera va dentro de network{}; "
                "quita el atmospheric de nivel superior"
            )
        # HALLAZGO F-2 (revision final SP-5): "r_system_ns2m8" es exclusivo
        # del modo "simple" - en modo "red" la resistencia la define la red
        # (network{}). Aceptarlo en modo "red" lo descartaria en silencio
        # (exit 0), igual riesgo que el atmospheric de nivel superior arriba.
        if "r_system_ns2m8" in data:
            raise ValueError("en modo 'red' sobra 'r_system_ns2m8'")
        if "network" not in data:
            raise ValueError("modo 'red' requiere 'network'")
        if "fan_branch_id" not in data:
            raise ValueError("modo 'red' requiere 'fan_branch_id'")
        network, atm, solver_params = _build_red_request(data["network"])
        result = ventpy.FanCalculator.operating_point_in_network(
            network, data["fan_branch_id"], curve, atm, solver_params,
            params)
    else:
        raise ValueError(
            f"modo desconocido '{mode}' (validas: simple, red)")

    _emit(
        _fan_operating_report(result), args.json,
        text_printer=lambda r: _print_ventilador_table(r, params.stall_margin),
    )
    return _flags_exit({
        "converged": result.converged,
        "stall_ok": result.stall_ok,
        "in_curve_range": result.in_curve_range,
    })


# ============================================================================
# Parser / entrypoint
# ============================================================================
_EXIT_CODES_EPILOG = (
    "Exit codes: 0 ok | 1 error de entrada | 2 resultado NO confiable "
    "(no convergio / no cumple / fuera de catalogo)"
)


def build_parser():
    parser = argparse.ArgumentParser(
        prog="ventpy",
        description="VentPy - calculos de ventilacion de minas subterraneas "
                     "(DS 024-2016-EM Peru / DS 132 Chile).",
        epilog=_EXIT_CODES_EPILOG,
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

    cobertura = subparsers.add_parser(
        "cobertura",
        help="Analisis de deficit/cobertura de un levantamiento de zonas",
        epilog=_EXIT_CODES_EPILOG)
    cobertura.add_argument("archivo", help="JSON con el levantamiento (zones/params)")
    cobertura.add_argument(
        "--norma", choices=sorted(_NORMAS), default="peru",
        help="Norma regulatoria a aplicar (default: peru)")
    cobertura.add_argument(
        "--json", action="store_true", help="Salida en formato JSON")
    cobertura.set_defaults(func=cmd_cobertura)

    red = subparsers.add_parser(
        "red", help="Balance de una red de ventilacion (Hardy Cross)",
        epilog=_EXIT_CODES_EPILOG)
    red.add_argument("archivo", help="JSON con la red (branches/solver/atmospheric)")
    red.add_argument(
        "--json", action="store_true", help="Salida en formato JSON")
    red.set_defaults(func=cmd_red)

    ventilador = subparsers.add_parser(
        "ventilador",
        help="Punto de operacion de un ventilador (simple o en red)",
        epilog=_EXIT_CODES_EPILOG)
    ventilador.add_argument(
        "archivo", help="JSON con la curva y el modo (simple/red)")
    ventilador.add_argument(
        "--json", action="store_true", help="Salida en formato JSON")
    ventilador.set_defaults(func=cmd_ventilador)

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
    except (ValueError, OSError, TypeError) as exc:
        # ValueError: whitelist/enum invalidos o excepciones del nucleo C++
        # (nanobind traduce std::invalid_argument -> ValueError).
        # OSError: archivo de entrada inexistente/sin permisos/ruta invalida
        # (FileNotFoundError, PermissionError, etc. heredan de OSError).
        # TypeError: red de seguridad (revision final SP-5, hallazgo F-1)
        # para formas de JSON que ni `_build` ni un builder a medida
        # alcanzan a traducir a ValueError (ej. un punto de curva escalar
        # donde se esperaba [q, p]: falla en un len() interno con
        # TypeError). Mejor un "error:" limpio que un traceback crudo.
        print(f"error: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
