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
