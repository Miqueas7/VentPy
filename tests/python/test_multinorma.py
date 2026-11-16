import pytest
import ventpy


class TestPresets:
    def test_chile_values(self):
        c = ventpy.RegulatoryConfig.chile()
        assert c.standard == ventpy.RegulatoryStandard.DS132_Chile
        assert c.min_flow_per_person == 3.0
        assert c.flow_above_threshold_3 == 3.0     # sin escalón por altitud
        assert c.diesel_hp_factor == 2.83          # Art. 132

    def test_peru_matches_defaults_art247(self):
        p = ventpy.RegulatoryConfig.peru()
        assert (p.altitude_threshold_1, p.flow_above_threshold_1) == (1500.0, 4.0)
        assert (p.altitude_threshold_2, p.flow_above_threshold_2) == (3000.0, 5.0)
        assert (p.altitude_threshold_3, p.flow_above_threshold_3) == (4000.0, 6.0)

    def test_for_standard_dispatch(self):
        assert ventpy.RegulatoryConfig.for_standard(
            ventpy.RegulatoryStandard.DS132_Chile).diesel_hp_factor == 2.83

    def test_colombia_values(self):
        c = ventpy.RegulatoryConfig.colombia()
        assert c.standard == ventpy.RegulatoryStandard.D1886_Colombia
        assert c.min_flow_per_person == 3.0        # Art. 54, num. 1
        assert (c.altitude_threshold_1, c.flow_above_threshold_1) == (1500.0, 6.0)
        assert c.flow_above_threshold_3 == 6.0     # un solo escalón
        assert c.diesel_hp_factor == 6.0           # Art. 54, par. 2, num. 1

    def test_colombia_escalon_unico_en_1500(self):
        c = ventpy.RegulatoryConfig.colombia()
        por_persona = lambda alt: ventpy.calculate_personnel_flow(
            10, alt, c).flow_per_person_base
        assert por_persona(1499.5) == 3.0
        assert por_persona(1500.0) == 6.0          # borde: rige la banda alta
        assert por_persona(4200.0) == 6.0

    def test_mexico_values(self):
        m = ventpy.RegulatoryConfig.mexico()
        assert m.standard == ventpy.RegulatoryStandard.NOM023_Mexico
        assert m.min_flow_per_person == 1.5        # 8.4.4 a) 1)
        assert m.flow_above_threshold_3 == 1.5     # sin escalón por altitud
        assert m.diesel_hp_factor == 2.13          # 8.4.4 a) 2)
        r = ventpy.calculate_personnel_flow(15, 4200.0, m)
        assert r.q_personnel == 23.0               # ceil(15 × 1,5)

    def test_for_standard_dispatch_colombia_mexico(self):
        for_standard = ventpy.RegulatoryConfig.for_standard
        assert for_standard(
            ventpy.RegulatoryStandard.D1886_Colombia).diesel_hp_factor == 6.0
        assert for_standard(
            ventpy.RegulatoryStandard.NOM023_Mexico).diesel_hp_factor == 2.13


class TestLmp:
    def test_peru_co(self):
        co = ventpy.lmp_for(ventpy.RegulatoryStandard.DS024_Peru, ventpy.GasType.CO)
        assert co.twa_8h == 25.0
        assert co.stel is None
        assert "Anexo 15" in co.regulation_ref

    def test_chile_co_44(self):
        co = ventpy.lmp_for(ventpy.RegulatoryStandard.DS132_Chile, ventpy.GasType.CO)
        assert co.twa_8h == 44.0
        assert "40 ppm" in co.regulation_ref

    def test_chile_no_lpp_22(self):
        # DS 594, Art. 66, 'Oxido Nitrico': LPP 22 ppm.
        no = ventpy.lmp_for(ventpy.RegulatoryStandard.DS132_Chile, ventpy.GasType.NO)
        assert no.twa_8h == 22.0
        assert "DS 594, Art. 66" in no.regulation_ref

    def test_tables_complete(self):
        assert len(ventpy.gas_limits(ventpy.RegulatoryStandard.DS024_Peru)) == 8
        assert len(ventpy.gas_limits(ventpy.RegulatoryStandard.DS132_Chile)) == 8
        assert len(ventpy.gas_limits(ventpy.RegulatoryStandard.D1886_Colombia)) == 8
        assert len(ventpy.gas_limits(ventpy.RegulatoryStandard.NOM023_Mexico)) == 8

    @pytest.mark.parametrize("standard, marca", [
        (ventpy.RegulatoryStandard.D1886_Colombia, "Decreto 1886 de 2015, Art. 39"),
        (ventpy.RegulatoryStandard.NOM023_Mexico, "NOM-010-STPS-2014, Tabla I.1"),
    ])
    def test_colombia_y_mexico_misma_tabla_de_gases(self, standard, marca):
        valores = {
            ventpy.GasType.CO: (25.0, None),
            ventpy.GasType.CO2: (5000.0, 30000.0),
            ventpy.GasType.NO2: (0.2, None),
            ventpy.GasType.NO: (25.0, None),
            ventpy.GasType.H2S: (1.0, 5.0),
            ventpy.GasType.SO2: (None, 0.25),      # solo corto tiempo: TWA vacío
        }
        for gas, (twa, stel) in valores.items():
            limite = ventpy.lmp_for(standard, gas)
            assert (limite.twa_8h, limite.stel) == (twa, stel), gas
            assert marca in limite.regulation_ref

    def test_colombia_ch4_y_o2(self):
        col = ventpy.RegulatoryStandard.D1886_Colombia
        ch4 = ventpy.lmp_for(col, ventpy.GasType.CH4)
        assert ch4.unit == ventpy.ConcentrationUnit.PercentVolume
        assert ch4.ceiling == 1.0 and "Art. 53" in ch4.regulation_ref
        o2 = ventpy.lmp_for(col, ventpy.GasType.O2)
        assert (o2.floor_min, o2.ceiling) == (19.5, 23.5)

    def test_mexico_ch4_higienico_y_o2_declarado(self):
        mex = ventpy.RegulatoryStandard.NOM023_Mexico
        ch4 = ventpy.lmp_for(mex, ventpy.GasType.CH4)
        assert ch4.twa_8h == 1000.0 and ch4.ceiling is None
        o2 = ventpy.lmp_for(mex, ventpy.GasType.O2)
        assert o2.floor_min == 19.5 and o2.ceiling is None
        assert "Criterio de ingenieria" in o2.regulation_ref


class TestAnexo38:
    """Total de mina con el DS 024: la voladura sigue el Anexo 38."""

    def _mina(self, **blast):
        inp = ventpy.VentilationInput()
        inp.zone_type = ventpy.ZoneType.GeneralMine
        b = ventpy.BlastingParams()
        b.explosive_kg = 50.0
        b.dilution_time_min = 30.0
        for k, v in blast.items():
            setattr(b, k, v)
        inp.blasting_params = b
        return inp

    def test_sin_diesel_rige_q_ex(self):
        gov = ventpy.VentilationGovernor(ventpy.RegulatoryConfig.peru())
        r = gov.calculate_total_demand(self._mina(blasting_levels=3))
        # 12 m2 x 25 m/min (ANFO, Art. 248) x 3 niveles
        assert r.q_blasting_m3min == 900.0
        assert r.q_total_m3min == 900.0
        assert r.governing_factor == "blasting (Q_Ex, Anexo 38)"

    def test_velocidad_maxima_advierte_en_el_frente(self):
        gov = ventpy.VentilationGovernor(ventpy.RegulatoryConfig.peru())
        inp = self._mina()
        inp.zone_type = ventpy.ZoneType.DevelopmentFace
        r = gov.calculate_total_demand(inp)
        assert any("maximo" in w and "Art. 248" in w for w in r.warnings)


class TestCorreccionesNormativas:
    """Objetivos y pisos que siguen a la norma configurada."""

    def test_simultaneidad_por_defecto_respeta_el_anexo_38(self):
        # Q_Eq = 3 x HP x Dm x Fu = 3 x 100 = 300 (DS 024-2016-EM, Anexo 38).
        vi = ventpy.VentilationInput()
        assert vi.simultaneity_factor == 1.0
        vi.zone_type = ventpy.ZoneType.GeneralMine
        vi.num_workers = 1
        vi.face_area_m2 = 1.0
        e = ventpy.DieselEquipment()
        e.name = "E1"
        e.horsepower = 100.0
        e.emission_tier = ventpy.EngineEmissionTier.Tier4_Final
        f = ventpy.DieselFleet()
        f.add_equipment(e)
        vi.diesel_fleet = f
        gov = ventpy.VentilationGovernor(ventpy.RegulatoryConfig.peru())
        assert gov.calculate_total_demand(vi).q_diesel_m3min == 300.0
        vi.simultaneity_factor = 0.85
        r = gov.calculate_total_demand(vi)
        assert r.q_diesel_m3min == 255.0
        assert any("simultaneidad 0.85" in w for w in r.warnings)

    @pytest.mark.parametrize("pct, q_ma", [
        (10.0, 0.0), (30.0, 60.0), (50.0, 100.0), (80.0, 125.0)])
    def test_q_ma_cuatro_tramos(self, pct, q_ma):
        # Q_Ma = T x u con T = 100 t/guardia (Anexo 38 lit. b).
        vi = ventpy.VentilationInput()
        vi.zone_type = ventpy.ZoneType.GeneralMine
        vi.num_workers = 10
        vi.face_area_m2 = 1.0
        vi.production_t_per_shift = 100.0
        vi.timber_consumption_pct = pct
        vi.hot_levels = 0
        r = ventpy.VentilationGovernor(
            ventpy.RegulatoryConfig.peru()).calculate_total_demand(vi)
        assert r.q_timber_m3min == q_ma
        assert r.q_governing_m3min == 30.0 + q_ma   # Q_Tr = 10 x 3

    def test_q_te_y_aviso_sin_datos(self):
        # Q_Te = 30 x 12 x 2 = 720 (Anexo 38 lit. c).
        vi = ventpy.VentilationInput()
        vi.zone_type = ventpy.ZoneType.GeneralMine
        vi.num_workers = 10
        vi.hot_levels = 2
        gov = ventpy.VentilationGovernor(ventpy.RegulatoryConfig.peru())
        r = gov.calculate_total_demand(vi)
        assert r.q_temperature_m3min == 720.0
        assert any("no incluye Q_Ma" in w for w in r.warnings)
        vi.hot_levels = None
        r = gov.calculate_total_demand(vi)
        assert r.q_temperature_m3min == 0.0
        assert any("no incluye Q_Te" in w for w in r.warnings)

    def test_objetivo_de_dilucion_de_la_norma(self):
        b = ventpy.BlastingParams()
        b.explosive_kg = 60.0
        b.dilution_time_min = 30.0
        assert b.target_nox_ppm == 0.0   # 0 = TWA de la norma
        pe = ventpy.calculate_blasting_flow(b, ventpy.RegulatoryConfig.peru())
        mx = ventpy.calculate_blasting_flow(b, ventpy.RegulatoryConfig.mexico())
        assert pe.target_nox_ppm == 3.0
        assert mx.target_nox_ppm == 0.2
        # 600 L / (0.2e-6 x 30) / 1000 x 1.2 = 120 000
        assert mx.q_blasting == 120000.0

    def test_cobertura_limites_por_norma(self):
        p = ventpy.CoverageParams()
        lim = ventpy.CoverageCalculator.velocity_limits
        assert lim(p, ventpy.RegulatoryConfig.peru()) == (20.0, 250.0)
        assert lim(p, ventpy.RegulatoryConfig.chile()) == (15.0, 150.0)
        assert lim(p, ventpy.RegulatoryConfig.colombia()) == (15.0, 360.0)
        mn, mx = lim(p, ventpy.RegulatoryConfig.mexico())
        assert mn == 15.24 and mx == float("inf")

    def test_polvo_chile_con_fa(self):
        # 2.4 x Fa(3000 msnm = 0.69) = 1.656 mg/m3 (DS 594, Arts. 63 y 66).
        d = ventpy.DustParams()
        d.dust_generation_rate_mg_s = 10.0
        atm = ventpy.AtmosphericParams()
        atm.altitude_masl = 3000.0
        r = ventpy.calculate_dust_flow(d, ventpy.RegulatoryConfig.chile(), atm)
        assert r.target_concentration == pytest.approx(1.656)
        assert ventpy.calculate_dust_flow(
            d, ventpy.RegulatoryConfig.peru()).target_concentration == 3.0


class TestFueraDelPresetBase:
    def test_pequena_mineria_chilena(self):
        cfg = ventpy.RegulatoryConfig.chile_small_mining()
        assert cfg.small_mining and cfg.diesel_hp_factor == 3.0
        f = ventpy.DieselFleet()
        f.add_equipment("Scoop", 100.0, 1.0, 1.0)
        assert "Art. 627" in ventpy.calculate_diesel_flow(f, cfg).regulation_ref

    def test_silice_fija_el_objetivo(self):
        dp = ventpy.DustParams()
        dp.dust_generation_rate_mg_s = 15.0
        dp.silica_content_percent = 100.0
        r = ventpy.calculate_dust_flow(dp, ventpy.RegulatoryConfig.chile())
        assert r.target_concentration == 0.08

    def test_tajo_de_carbon_colombia(self):
        inp = ventpy.VentilationInput()
        inp.zone_type = ventpy.ZoneType.Stope
        inp.num_workers = 1200
        inp.coal_mine = True
        r = ventpy.VentilationGovernor(ventpy.RegulatoryConfig.colombia()).calculate_total_demand(inp)
        assert any("maximo 270" in w for w in r.warnings)
