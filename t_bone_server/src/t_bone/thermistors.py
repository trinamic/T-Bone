# coding=utf-8
import logging
from t_bone import ramps_thermistors
from t_bone import replicape_thermistors

_logger = logging.getLogger(__name__)



def get_thermistor_reading(thermistor, value):
    #find the thermistor
    thermistor_table = None
    if thermistor == "100k":
        return ramps_thermistors.convert_ramps_reading(thermistor, ramps_thermistors.bed_thermistor_100k, value)
    elif thermistor == "200k":
        return ramps_thermistors.convert_ramps_reading(thermistor, ramps_thermistors.bed_thermistor_200k, value)
    elif thermistor == "mendel-parts":
        return ramps_thermistors.convert_ramps_reading(thermistor, ramps_thermistors.mendel_parts_thermistor, value)
    elif thermistor == "10k":
        return ramps_thermistors.convert_ramps_reading(thermistor, ramps_thermistors.thermistor_10k, value)
    elif thermistor == "parcan-100k":
        return ramps_thermistors.convert_ramps_reading(thermistor, ramps_thermistors.thermistor_parcan_100k, value)
    elif thermistor == "epcos-100k":
        return ramps_thermistors.convert_ramps_reading(thermistor, ramps_thermistors.thermistor_epcos_100k, value)
    elif thermistor == "epcos-B57560G104F":
        return ramps_thermistors.convert_ramps_reading(thermistor, ramps_thermistors.thermistor_epcos_B57560G104F, value)
    elif thermistor == "j-head":
        return ramps_thermistors.convert_ramps_reading(thermistor, ramps_thermistors.j_head_thermistor, value)
    elif thermistor == "honeywell-100k":
        return ramps_thermistors.convert_ramps_reading(thermistor, ramps_thermistors.thermistor_honeywell_100k, value)
    elif thermistor == "honeywell-135_104_LAF_J01":
        return ramps_thermistors.convert_ramps_reading(thermistor, ramps_thermistors.thermistor_honeywell_135_104_LAF_J01, value)
    elif thermistor == "vishay-NTCS0603E3104FXT":
        return ramps_thermistors.convert_ramps_reading(thermistor, ramps_thermistors.thermistor_vishay_NTCS0603E3104FXT, value)
    elif thermistor == "ge-sensing":
        return ramps_thermistors.convert_ramps_reading(thermistor, ramps_thermistors.thermistor_ge_sensing, value)
    elif thermistor == "rs-198961":
        return ramps_thermistors.convert_ramps_reading(thermistor, ramps_thermistors.thermistor_rs_198961, value)
    elif thermistor == "B57560G104F":
        return replicape_thermistors.convert_reading(replicape_thermistors.temp_table["B57560G104F"], value)
    if not thermistor:
        raise Exception("Unknown Thermistor" + thermistor)

