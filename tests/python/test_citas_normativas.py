"""
La referencia normativa de cada resultado sigue a la norma configurada.

Con el preajuste peruano la cita apunta al DS 024-2016-EM / DS 023-2017-EM.
Con el preajuste chileno la cita apunta al DS 132 o declara explicitamente que
el DS 132 no fija el valor; en ningun caso aparece la normativa peruana.
"""

import ventpy


PERU = ventpy.RegulatoryConfig.peru()
CHILE = ventpy.RegulatoryConfig.chile()

# Marcas de la normativa peruana que jamas deben aparecer bajo marco chileno.
MARCAS_PERU = ("DS 024", "DS 023", "DS 015-2005-SA", "Anexo 15", "Anexo 13")


def sin_normativa_peruana(texto):
    return [m for m in MARCAS_PERU if m in texto]


# ---------------------------------------------------------------------------
# Constructores de entrada compartidos
# ---------------------------------------------------------------------------


def flota_diesel():
    f = ventpy.DieselFleet()
    f.add_equipment("Scooptram", 200.0, 0.85, 0.70)
    return f


def params_voladura():
    p = ventpy.BlastingParams()
    p.explosive_kg = 50.0
    p.dilution_time_min = 45.0          # > 30 => dispara la ADVERTENCIA
    p.face_area_m2 = 12.0
    p.face_length_m = 100.0
    p.gas_volume_per_kg = 0.04
    return p


def params_polvo():
    p = ventpy.DustParams()
    p.dust_generation_rate_mg_s = 50.0
    p.target_concentration_mg_m3 = 4.0  # > 3 => dispara la advertencia de LEO
    p.silica_content_percent = 12.0     # dispara la remision de silice
    p.face_area_m2 = 12.0
    return p


def params_termicos():
    p = ventpy.ThermalParams()
    p.depth_below_surface_m = 900.0
    p.auto_compression_c_per_100m = 0.98
    p.heat_from_equipment_kw = 400.0
    p.heat_from_oxidation_kw = 50.0
    p.target_effective_temp_c = 28.0
    p.face_area_m2 = 12.0
    return p


def atmosfera_2500_16c():
    a = ventpy.AtmosphericParams()
    a.altitude_masl = 2500.0
    a.dry_bulb_temp_c = 16.0
    return a


# ---------------------------------------------------------------------------
# Cita por calculador
# ---------------------------------------------------------------------------


class TestCaudalPersonal:
    def test_peru_cita_art_236(self):
        r = ventpy.calculate_personnel_flow(15, 4200.0, PERU)
        assert "DS 024-2016-EM, Art. 236" in r.regulation_ref

    def test_chile_cita_art_138(self):
        r = ventpy.calculate_personnel_flow(15, 4200.0, CHILE)
        assert "DS 132, Art. 138" in r.regulation_ref
        assert sin_normativa_peruana(r.regulation_ref) == []

    def test_chile_no_cita_escala_de_altitud(self):
        # El DS 132 fija 3,0 m3/min sin escalon por altitud: no hay Art. 247.
        r = ventpy.calculate_personnel_flow(15, 4200.0, CHILE)
        assert "247" not in r.regulation_ref


class TestCaudalEquipo:
    def test_peru_cita_art_246(self):
        r = ventpy.calculate_diesel_flow(flota_diesel(), PERU)
        assert "DS 024-2016-EM, Art. 246" in r.regulation_ref

    def test_chile_cita_art_132(self):
        r = ventpy.calculate_diesel_flow(flota_diesel(), CHILE)
        assert "DS 132, Art. 132" in r.regulation_ref
        assert sin_normativa_peruana(r.regulation_ref) == []


class TestCaudalExplosivos:
    def test_peru_cita_art_243_244(self):
        r = ventpy.calculate_blasting_flow(params_voladura(), PERU)
        assert "DS 024-2016-EM, Art. 243-244" in r.regulation_ref
        assert "ADVERTENCIA" in r.regulation_ref
        assert "max normativo" in r.regulation_ref

    def test_chile_cita_reingreso_y_declara_vacio(self):
        r = ventpy.calculate_blasting_flow(params_voladura(), CHILE)
        assert "DS 132, Arts. 156, 571 y 585" in r.regulation_ref
        assert "no fija tiempo de dilucion" in r.regulation_ref
        assert sin_normativa_peruana(r.regulation_ref) == []

    def test_chile_advertencia_no_llama_normativo_al_tope(self):
        r = ventpy.calculate_blasting_flow(params_voladura(), CHILE)
        assert "ADVERTENCIA" in r.regulation_ref
        assert "max configurado" in r.regulation_ref
        assert "max normativo" not in r.regulation_ref


class TestCaudalPolvo:
    def test_peru_cita_art_111(self):
        r = ventpy.calculate_dust_flow(params_polvo(), PERU)
        assert "DS 024-2016-EM, Art. 111" in r.regulation_ref
        assert any("Anexo 15" in w for w in r.warnings)

    def test_chile_declara_vacio(self):
        r = ventpy.calculate_dust_flow(params_polvo(), CHILE)
        assert "DS 132" in r.regulation_ref
        assert "sin limite de polvo respirable verificado" in r.regulation_ref
        assert sin_normativa_peruana(r.regulation_ref) == []

    def test_chile_advertencias_sin_articulo_peruano(self):
        r = ventpy.calculate_dust_flow(params_polvo(), CHILE)
        assert r.warnings, "el caso debe disparar advertencias de silice y de LEO"
        for w in r.warnings:
            assert sin_normativa_peruana(w) == []
            assert "Art. 111" not in w


class TestCaudalTermico:
    def test_peru_cita_art_252d(self):
        r = ventpy.calculate_thermal_flow(
            params_termicos(), atmosfera_2500_16c(), PERU)
        assert "DS 024-2016-EM, Art. 252.d" in r.regulation_ref

    def test_chile_declara_criterio_de_ingenieria(self):
        r = ventpy.calculate_thermal_flow(
            params_termicos(), atmosfera_2500_16c(), CHILE)
        assert "DS 132" in r.regulation_ref
        assert "criterio de ingenieria" in r.regulation_ref
        assert "252" not in r.regulation_ref
        assert sin_normativa_peruana(r.regulation_ref) == []

    def test_chile_remision_estres_termico_sin_anexo_peruano(self):
        p = params_termicos()
        p.face_area_m2 = 0.0
        p.depth_below_surface_m = 1000.0
        a = ventpy.AtmosphericParams()
        a.altitude_masl = 2500.0
        a.dry_bulb_temp_c = 25.0            # inlet = 34.8 > 29
        r = ventpy.calculate_thermal_flow(p, a, CHILE)
        assert any("estres termico" in w for w in r.warnings)
        for w in r.warnings:
            assert sin_normativa_peruana(w) == []
            assert "Art. 104" not in w


class TestCobertura:
    @staticmethod
    def medicion(nombre="Frente N-02", medido=90.0):
        m = ventpy.ZoneMeasurement()
        m.zone_name = nombre
        m.q_measured_m3min = medido
        return m

    def test_peru_cita_art_252_g(self):
        r = ventpy.CoverageCalculator.compare_zone(
            100.0, self.medicion(), config=PERU)
        assert "Art. 252 lit. g" in r.regulation_ref

    def test_chile_declara_vacio(self):
        r = ventpy.CoverageCalculator.compare_zone(
            100.0, self.medicion(), config=CHILE)
        assert "DS 132" in r.regulation_ref
        assert "criterio de ingenieria" in r.regulation_ref
        assert sin_normativa_peruana(r.regulation_ref) == []

    def test_chile_estacion_fuera_de_rango_no_cita_art_248(self):
        m = ventpy.ZoneMeasurement()
        m.zone_name = "Frente N-02"
        e = ventpy.AirflowStation()
        e.station_id = "E-1"
        e.area_m2 = 12.0
        e.velocity_mps = 0.30               # 18 m/min, por debajo del minimo
        m.stations = [e]
        r = ventpy.CoverageCalculator.compare_zone(100.0, m, config=CHILE)
        assert r.stations[0].velocity_ok is False
        assert "248" not in r.stations[0].warning
        assert sin_normativa_peruana(r.stations[0].warning) == []

    def test_chile_balance_de_mina_declara_vacio(self):
        z = ventpy.ZoneSurvey()
        z.zone_name = "Frente N-02"
        z.input = ventpy.VentilationInput()
        z.input.zone_type = ventpy.ZoneType.DevelopmentFace
        z.input.num_workers = 10
        z.input.altitude_masl = 4200.0
        z.measurement = ventpy.ZoneMeasurement()
        z.measurement.zone_name = "Frente N-02"
        z.measurement.q_measured_m3min = 1000.0

        r = ventpy.CoverageCalculator.analyze_survey([z], CHILE)
        assert "DS 132" in r.regulation_ref
        assert sin_normativa_peruana(r.regulation_ref) == []
        assert sin_normativa_peruana(r.zones[0].regulation_ref) == []


# ---------------------------------------------------------------------------
# Red transversal: ningun resultado alcanzable cita la normativa peruana
# bajo marco chileno. Es la prueba que atrapa un calculador nuevo que
# reintroduzca el defecto.
# ---------------------------------------------------------------------------


def entrada_completa():
    inp = ventpy.VentilationInput()
    inp.zone_type = ventpy.ZoneType.DevelopmentFace
    inp.face_area_m2 = 12.0
    inp.face_length_m = 100.0
    inp.num_workers = 15
    inp.atmospheric = atmosfera_2500_16c()
    inp.diesel_fleet = flota_diesel()
    inp.blasting_params = params_voladura()
    inp.dust_params = params_polvo()
    inp.thermal_params = params_termicos()
    return inp


def textos_auditables(demanda):
    """Toda cadena de auditoria alcanzable desde un resultado del Governor."""
    textos = list(demanda.warnings)
    for sub in (demanda.personnel, demanda.diesel, demanda.blasting,
                demanda.dust, demanda.thermal):
        if sub is None:
            continue
        textos.append(sub.regulation_ref)
        textos.extend(getattr(sub, "warnings", []))
    if demanda.leakage is not None:
        textos.append(demanda.leakage.notes)
    return textos


class TestBarridoTransversal:
    def test_governor_chileno_no_menciona_normativa_peruana(self):
        demanda = ventpy.VentilationGovernor(CHILE).calculate_total_demand(
            entrada_completa())
        sub_presentes = [s for s in (demanda.personnel, demanda.diesel,
                                     demanda.blasting, demanda.dust,
                                     demanda.thermal) if s is not None]
        assert len(sub_presentes) == 5, "el barrido debe cubrir los 5 caudales"

        infracciones = []
        for texto in textos_auditables(demanda):
            for marca in sin_normativa_peruana(texto):
                infracciones.append((marca, texto))
        assert infracciones == []

    def test_governor_peruano_conserva_su_cita(self):
        demanda = ventpy.VentilationGovernor(PERU).calculate_total_demand(
            entrada_completa())
        refs = [s.regulation_ref for s in (demanda.personnel, demanda.diesel,
                                           demanda.blasting, demanda.dust,
                                           demanda.thermal)]
        assert all("DS 024" in r for r in refs)

    def test_cada_cita_chilena_nombra_su_marco(self):
        demanda = ventpy.VentilationGovernor(CHILE).calculate_total_demand(
            entrada_completa())
        for sub in (demanda.personnel, demanda.diesel, demanda.blasting,
                    demanda.dust, demanda.thermal):
            assert "DS 132" in sub.regulation_ref
