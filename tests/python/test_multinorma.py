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
        assert por_persona(1500.0) == 3.0          # borde: rige la banda inferior
        assert por_persona(1500.5) == 6.0
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

    def test_chile_no_not_regulated(self):
        with pytest.raises(ValueError):
            ventpy.lmp_for(ventpy.RegulatoryStandard.DS132_Chile, ventpy.GasType.NO)

    def test_tables_complete(self):
        assert len(ventpy.gas_limits(ventpy.RegulatoryStandard.DS024_Peru)) == 8
        assert len(ventpy.gas_limits(ventpy.RegulatoryStandard.DS132_Chile)) == 7
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
