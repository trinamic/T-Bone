# coding=utf-8
#thermistor table taken from current marlin software
#see https:  #raw.github.com/ErikZalm/Marlin/Marlin_v1/Marlin/thermistortables.h
from numpy import NaN


def get_thermistor_reading(thermistor, raw_value):
    #find the thermistor
    thermistortable = None
    if thermistor == "100k":
        thermistortable = bed_thermistor_100k
    elif thermistor == "200k":
        thermistortable = bed_thermistor_200k
    elif thermistor == "mendel-parts":
        thermistortable = mendel_parts_thermistor
    elif thermistor == "10k":
        thermistortable = thermistor_10k
    elif thermistor == "parcan-100k":
        thermistortable = thermistor_parcan_100k
    elif thermistor == "epcos-100k":
        thermistortable = thermistor_epcos_100k
    elif thermistor == "honeywell-100k":
        thermistortable = thermistor_honeywell_100k
    elif thermistor == "honeywell-135_104_LAF_J01":
        thermistortable = thermistor_honeywell_135_104_LAF_J01
    elif thermistor == "vishay-NTCS0603E3104FXT":
        thermistortable = thermistor_vishay_NTCS0603E3104FXT
    elif thermistor == "ge-sensing":
        thermistortable = thermistor_ge_sensing
    elif thermistor == "rs-198961":
        thermistortable = thermistor_rs_198961
    if not thermistor:
        raise Exception("Unknown Thermistor" + thermistor)

    #the tables are from 1024er based arduino
    comparable_value = raw_value / 4.0
    #find upper value
    upper_value = int(comparable_value)
    upper_temperature = None
    while upper_value < 1024:
        if upper_value in thermistortable:
            upper_temperature = thermistortable[upper_value]
            break
        upper_value += 1
    lower_value = int(comparable_value)
    lower_temperature = None
    while lower_value >= 0:
        if lower_value in thermistortable:
            lower_temperature = thermistortable[lower_value]
            break
        lower_value -= 1

    #now decode it
    if upper_temperature and lower_temperature:
        value_difference = float(upper_value - lower_value)
        temperature_difference = float(upper_temperature - lower_temperature)
        return float(lower_temperature) \
               + temperature_difference / value_difference \
                 * (raw_value - lower_value) / value_difference
    elif lower_temperature:
        return lower_value
    elif upper_temperature:
        return upper_temperature
    else:
        #todo log or so?
        return NaN


#100k bed thermistor
bed_thermistor_100k = {
    23: 300,
    25: 295,
    27: 290,
    28: 285,
    31: 280,
    33: 275,
    35: 270,
    38: 265,
    41: 260,
    44: 255,
    48: 250,
    52: 245,
    56: 240,
    61: 235,
    66: 230,
    71: 225,
    78: 220,
    84: 215,
    92: 210,
    100: 205,
    109: 200,
    120: 195,
    131: 190,
    143: 185,
    156: 180,
    171: 175,
    187: 170,
    205: 165,
    224: 160,
    245: 155,
    268: 150,
    293: 145,
    320: 140,
    348: 135,
    379: 130,
    411: 125,
    445: 120,
    480: 115,
    516: 110,
    553: 105,
    591: 100,
    628: 95,
    665: 90,
    702: 85,
    737: 80,
    770: 75,
    801: 70,
    830: 65,
    857: 60,
    881: 55,
    903: 50,
    922: 45,
    939: 40,
    954: 35,
    966: 30,
    977: 25,
    985: 20,
    993: 15,
    999: 10,
    1004: 5,
    1008: 0  #safety
}

#200k bed thermistor
bed_thermistor_200k = {
    #200k ATC Semitec 204GT-2
    #Verified by linagee. Source: http:  #shop.arcol.hu/static/datasheets/thermistors.pdf
    # Calculated using 4.7kohm pullup, voltage divider math, and manufacturer provided temp/resistance
    1: 848,
    30: 300,  #top rating 300C
    34: 290,
    39: 280,
    46: 270,
    53: 260,
    63: 250,
    74: 240,
    87: 230,
    104: 220,
    124: 210,
    148: 200,
    176: 190,
    211: 180,
    252: 170,
    301: 160,
    357: 150,
    420: 140,
    489: 130,
    562: 120,
    636: 110,
    708: 100,
    775: 90,
    835: 80,
    884: 70,
    924: 60,
    955: 50,
    977: 40,
    993: 30,
    1004: 20,
    1012: 10,
    1016: 0,
}

#mendel-parts
mendel_parts_thermistor = {
    1: 864,
    21: 300,
    25: 290,
    29: 280,
    33: 270,
    39: 260,
    46: 250,
    54: 240,
    64: 230,
    75: 220,
    90: 210,
    107: 200,
    128: 190,
    154: 180,
    184: 170,
    221: 160,
    265: 150,
    316: 140,
    375: 130,
    441: 120,
    513: 110,
    588: 100,
    734: 80,
    856: 60,
    938: 40,
    986: 20,
    1008: 0,
    1018: -20
}

#10k thermistor
thermistor_10k = {
    1: 430,
    54: 137,
    107: 107,
    160: 91,
    213: 80,
    266: 71,
    319: 64,
    372: 57,
    425: 51,
    478: 46,
    531: 41,
    584: 35,
    637: 30,
    690: 25,
    743: 20,
    796: 14,
    849: 7,
    902: 0,
    955: -11,
    1008: -35
}

##100k ParCan thermistor (104GT-2)
thermistor_parcan_100k = {
    # ATC Semitec 104GT-2 (Used in ParCan)
    # Verified by linagee. Source: http:  #shop.arcol.hu/static/datasheets/thermistors.pdf
    # Calculated using 4.7kohm pullup, voltage divider math, and manufacturer provided temp/resistance
    1: 713,
    17: 300,  #top rating 300C
    20: 290,
    23: 280,
    27: 270,
    31: 260,
    37: 250,
    43: 240,
    51: 230,
    61: 220,
    73: 210,
    87: 200,
    106: 190,
    128: 180,
    155: 170,
    189: 160,
    230: 150,
    278: 140,
    336: 130,
    402: 120,
    476: 110,
    554: 100,
    635: 90,
    713: 80,
    784: 70,
    846: 60,
    897: 50,
    937: 40,
    966: 30,
    986: 20,
    1000: 10,
    1010: 0
}

# 100k Epcos thermistor
thermistor_epcos_100k = {
    1: 350,
    28: 250,  #top rating 250C
    31: 245,
    35: 240,
    39: 235,
    42: 230,
    44: 225,
    49: 220,
    53: 215,
    62: 210,
    71: 205,  #fitted graphically
    78: 200,  #fitted graphically
    94: 190,
    102: 185,
    116: 170,
    143: 160,
    183: 150,
    223: 140,
    270: 130,
    318: 120,
    383: 110,
    413: 105,
    439: 100,
    484: 95,
    513: 90,
    607: 80,
    664: 70,
    781: 60,
    810: 55,
    849: 50,
    914: 45,
    914: 40,
    935: 35,
    954: 30,
    970: 25,
    978: 22,
    1008: 3,
    1023: 0  #to allow internal 0 degrees C
}

# 100k Honeywell 135-104LAG-J01
thermistor_honeywell_100k = {
    1: 941,
    19: 362,
    37: 299,  #top rating 300C
    55: 266,
    73: 245,
    91: 229,
    109: 216,
    127: 206,
    145: 197,
    163: 190,
    181: 183,
    199: 177,
    217: 171,
    235: 166,
    253: 162,
    271: 157,
    289: 153,
    307: 149,
    325: 146,
    343: 142,
    361: 139,
    379: 135,
    397: 132,
    415: 129,
    433: 126,
    451: 123,
    469: 121,
    487: 118,
    505: 115,
    523: 112,
    541: 110,
    559: 107,
    577: 105,
    595: 102,
    613: 99,
    631: 97,
    649: 94,
    667: 92,
    685: 89,
    703: 86,
    721: 84,
    739: 81,
    757: 78,
    775: 75,
    793: 72,
    811: 69,
    829: 66,
    847: 62,
    865: 59,
    883: 55,
    901: 51,
    919: 46,
    937: 41,
    955: 35,
    973: 27,
    991: 17,
    1009: 1,
    1023: 0  #to allow internal 0 degrees C
}
# 100k Honeywell 135-104LAF-J01
# R0 = 100000 Ohm
# T0 = 25 °C
# Beta = 3974
# R1 = 0 Ohm
# R2 = 4700 Ohm
thermistor_honeywell_135_104_LAF_J01 = {
    35: 300,
    51: 270,
    54: 265,
    58: 260,
    59: 258,
    61: 256,
    63: 254,
    64: 252,
    66: 250,
    67: 249,
    68: 248,
    69: 247,
    70: 246,
    71: 245,
    72: 244,
    73: 243,
    74: 242,
    75: 241,
    76: 240,
    77: 239,
    78: 238,
    79: 237,
    80: 236,
    81: 235,
    82: 234,
    84: 233,
    85: 232,
    86: 231,
    87: 230,
    89: 229,
    90: 228,
    91: 227,
    92: 226,
    94: 225,
    95: 224,
    97: 223,
    98: 222,
    99: 221,
    101: 220,
    102: 219,
    104: 218,
    106: 217,
    107: 216,
    109: 215,
    110: 214,
    112: 213,
    114: 212,
    115: 211,
    117: 210,
    119: 209,
    121: 208,
    123: 207,
    125: 206,
    126: 205,
    128: 204,
    130: 203,
    132: 202,
    134: 201,
    136: 200,
    139: 199,
    141: 198,
    143: 197,
    145: 196,
    147: 195,
    150: 194,
    152: 193,
    154: 192,
    157: 191,
    159: 190,
    162: 189,
    164: 188,
    167: 187,
    170: 186,
    172: 185,
    175: 184,
    178: 183,
    181: 182,
    184: 181,
    187: 180,
    190: 179,
    193: 178,
    196: 177,
    199: 176,
    202: 175,
    205: 174,
    208: 173,
    212: 172,
    215: 171,
    219: 170,
    237: 165,
    256: 160,
    300: 150,
    351: 140,
    470: 120,
    504: 115,
    538: 110,
    552: 108,
    566: 106,
    580: 104,
    594: 102,
    608: 100,
    622: 98,
    636: 96,
    650: 94,
    664: 92,
    678: 90,
    712: 85,
    745: 80,
    758: 78,
    770: 76,
    783: 74,
    795: 72,
    806: 70,
    818: 68,
    829: 66,
    840: 64,
    850: 62,
    860: 60,
    870: 58,
    879: 56,
    888: 54,
    897: 52,
    905: 50,
    924: 45,
    940: 40,
    955: 35,
    967: 30,
    970: 29,
    972: 28,
    974: 27,
    976: 26,
    978: 25,
    980: 24,
    982: 23,
    984: 22,
    985: 21,
    987: 20,
    995: 15,
    1001: 10,
    1006: 5,
    1010: 0
}
#endif

# 100k 0603 SMD Vishay NTCS0603E3104FXT (4.7k pullup)
thermistor_vishay_NTCS0603E3104FXT = {
    1: 704,
    54: 216,
    107: 175,
    160: 152,
    213: 137,
    266: 125,
    319: 115,
    372: 106,
    425: 99,
    478: 91,
    531: 85,
    584: 78,
    637: 71,
    690: 65,
    743: 58,
    796: 50,
    849: 42,
    902: 31,
    955: 17,
    1008: 0
}

# 100k GE Sensing AL03006-58.2K-97-G1 (4.7k pullup)
thermistor_ge_sensing = {
    1: 936,
    36: 300,
    71: 246,
    106: 218,
    141: 199,
    176: 185,
    211: 173,
    246: 163,
    281: 155,
    316: 147,
    351: 140,
    386: 134,
    421: 128,
    456: 122,
    491: 117,
    526: 112,
    561: 107,
    596: 102,
    631: 97,
    666: 92,
    701: 87,
    736: 81,
    771: 76,
    806: 70,
    841: 63,
    876: 56,
    911: 48,
    946: 38,
    981: 23,
    1005: 5,
    1016: 0
}

# 100k RS thermistor 198-961 (4.7k pullup)
thermistor_rs_198961 = {
    1: 929,
    36: 299,
    71: 246,
    106: 217,
    141: 198,
    176: 184,
    211: 173,
    246: 163,
    281: 154,
    316: 147,
    351: 140,
    386: 134,
    421: 128,
    456: 122,
    491: 117,
    526: 112,
    561: 107,
    596: 102,
    631: 97,
    666: 91,
    701: 86,
    736: 81,
    771: 76,
    806: 70,
    841: 63,
    876: 56,
    911: 48,
    946: 38,
    981: 23,
    1005: 5,
    1016: 0
}


# PT100 with INA826 amp on Ultimaker v2.0 electronics
# The PT100 in the Ultimaker v2.0 electronics has a high sample value for a high temperature.
# This does not match the normal thermistor behaviour so we need to set the following defines */
#if (THERMISTORHEATER_0 == 20)
# define HEATER_0_RAW_HI_TEMP 16383
# define HEATER_0_RAW_LO_TEMP 0
#endif
#if (THERMISTORHEATER_1 == 20)
# define HEATER_1_RAW_HI_TEMP 16383
# define HEATER_1_RAW_LO_TEMP 0
#endif
#if (THERMISTORHEATER_2 == 20)
# define HEATER_2_RAW_HI_TEMP 16383
# define HEATER_2_RAW_LO_TEMP 0
#endif
#if (THERMISTORBED == 20)
# define HEATER_BED_RAW_HI_TEMP 16383
# define HEATER_BED_RAW_LO_TEMP 0
#endif
thermistor_ultimaker_v2 = {
    0: 0,
    227: 1,
    236: 10,
    245: 20,
    253: 30,
    262: 40,
    270: 50,
    279: 60,
    287: 70,
    295: 80,
    304: 90,
    312: 100,
    320: 110,
    329: 120,
    337: 130,
    345: 140,
    353: 150,
    361: 160,
    369: 170,
    377: 180,
    385: 190,
    393: 200,
    401: 210,
    409: 220,
    417: 230,
    424: 240,
    432: 250,
    440: 260,
    447: 270,
    455: 280,
    463: 290,
    470: 300,
    478: 310,
    485: 320,
    493: 330,
    500: 340,
    507: 350,
    515: 360,
    522: 370,
    529: 380,
    537: 390,
    544: 400,
    614: 500,
    681: 600,
    744: 700,
    805: 800,
    862: 900,
    917: 1000,
    968: 1100
}

# 100k EPCOS (WITH 1kohm RESISTOR FOR PULLUP, R9 ON SANGUINOLOLU! NOT FOR 4.7kohm PULLUP! THIS IS NOT NORMAL!)
# Verified by linagee.
# Calculated using 1kohm pullup, voltage divider math, and manufacturer provided temp/resistance
# Advantage: Twice the resolution and better linearity from 150C to 200C
thermistor_epcos_100k_sanguinololu = {
    1: 350,
    190: 250,  #top rating 250C
    203: 245,
    217: 240,
    232: 235,
    248: 230,
    265: 225,
    283: 220,
    302: 215,
    322: 210,
    344: 205,
    366: 200,
    390: 195,
    415: 190,
    440: 185,
    467: 180,
    494: 175,
    522: 170,
    551: 165,
    580: 160,
    609: 155,
    638: 150,
    666: 145,
    695: 140,
    722: 135,
    749: 130,
    775: 125,
    800: 120,
    823: 115,
    845: 110,
    865: 105,
    884: 100,
    901: 95,
    917: 90,
    932: 85,
    944: 80,
    956: 75,
    966: 70,
    975: 65,
    982: 60,
    989: 55,
    995: 50,
    1000: 45,
    1004: 40,
    1007: 35,
    1010: 30,
    1013: 25,
    1015: 20,
    1017: 15,
    1018: 10,
    1019: 5,
    1020: 0,
    1021: -5
}

# 200k ATC Semitec 204GT-2 (WITH 1kohm RESISTOR FOR PULLUP, R9 ON SANGUINOLOLU! NOT FOR 4.7kohm PULLUP! THIS IS NOT NORMAL!)
# Verified by linagee. Source: http:  #shop.arcol.hu/static/datasheets/thermistors.pdf
# Calculated using 1kohm pullup, voltage divider math, and manufacturer provided temp/resistance
# Advantage: More resolution and better linearity from 150C to 200C
thermistor_atc_semitek_200k = {
    1: 500,
    125: 300,  #top rating 300C
    142: 290,
    162: 280,
    185: 270,
    211: 260,
    240: 250,
    274: 240,
    312: 230,
    355: 220,
    401: 210,
    452: 200,
    506: 190,
    563: 180,
    620: 170,
    677: 160,
    732: 150,
    783: 140,
    830: 130,
    871: 120,
    906: 110,
    935: 100,
    958: 90,
    976: 80,
    990: 70,
    1000: 60,
    1008: 50,
    1013: 40,
    1017: 30,
    1019: 20,
    1021: 10,
    1022: 0
}

# 100k ATC Semitec 104GT-2 (Used on ParCan) (WITH 1kohm RESISTOR FOR PULLUP, R9 ON SANGUINOLOLU! NOT FOR 4.7kohm PULLUP! THIS IS NOT NORMAL!)
# Verified by linagee. Source: http:  #shop.arcol.hu/static/datasheets/thermistors.pdf
# Calculated using 1kohm pullup, voltage divider math, and manufacturer provided temp/resistance
# Advantage: More resolution and better linearity from 150C to 200C
thermistor_atc_semitek_100k = {
    1: 500,
    76: 300,
    87: 290,
    100: 280,
    114: 270,
    131: 260,
    152: 250,
    175: 240,
    202: 230,
    234: 220,
    271: 210,
    312: 200,
    359: 190,
    411: 180,
    467: 170,
    527: 160,
    590: 150,
    652: 140,
    713: 130,
    770: 120,
    822: 110,
    867: 100,
    905: 90,
    936: 80,
    961: 70,
    979: 60,
    993: 50,
    1003: 40,
    1010: 30,
    1015: 20,
    1018: 10,
    1020: 0
}

# Maker's Tool Works Kapton Bed Thermister
thermistor_makers_tool_kapton_bed = {
    51: 272,
    61: 258,
    71: 247,
    81: 237,
    91: 229,
    101: 221,
    131: 204,
    161: 190,
    191: 179,
    231: 167,
    271: 157,
    311: 148,
    351: 140,
    381: 135,
    411: 130,
    441: 125,
    451: 123,
    461: 122,
    471: 120,
    481: 119,
    491: 117,
    501: 116,
    511: 114,
    521: 113,
    531: 111,
    541: 110,
    551: 108,
    561: 107,
    571: 105,
    581: 104,
    591: 102,
    601: 101,
    611: 100,
    621: 98,
    631: 97,
    641: 95,
    651: 94,
    661: 92,
    671: 91,
    681: 90,
    691: 88,
    701: 87,
    711: 85,
    721: 84,
    731: 82,
    741: 81,
    751: 79,
    761: 77,
    771: 76,
    781: 74,
    791: 72,
    801: 71,
    811: 69,
    821: 67,
    831: 65,
    841: 63,
    851: 62,
    861: 60,
    871: 57,
    881: 55,
    891: 53,
    901: 51,
    911: 48,
    921: 45,
    931: 42,
    941: 39,
    951: 36,
    961: 32,
    981: 23,
    991: 17,
    1001: 9,
    1008: 0,
}



#define _TT_NAME(_N) temptable_ ## _N
#define TT_NAME(_N) _TT_NAME(_N)

#ifdef THERMISTORHEATER_0
# define HEATER_0_TEMPTABLE TT_NAME(THERMISTORHEATER_0)
# define HEATER_0_TEMPTABLE_LEN (sizeof(HEATER_0_TEMPTABLE)/sizeof(*HEATER_0_TEMPTABLE))
#else
# ifdef HEATER_0_USES_THERMISTOR
#  error No heater 0 thermistor table specified
# else    # HEATER_0_USES_THERMISTOR
#  define HEATER_0_TEMPTABLE NULL
#  define HEATER_0_TEMPTABLE_LEN 0
# endif   # HEATER_0_USES_THERMISTOR
#endif

#Set the high and low raw values for the heater, this indicates which raw value is a high or low temperature
#ifndef HEATER_0_RAW_HI_TEMP
# ifdef HEATER_0_USES_THERMISTOR     #In case of a thermistor the highest temperature results in the lowest ADC value
#  define HEATER_0_RAW_HI_TEMP 0
#  define HEATER_0_RAW_LO_TEMP 16383
# else                            #In case of an thermocouple the highest temperature results in the highest ADC value
#  define HEATER_0_RAW_HI_TEMP 16383
#  define HEATER_0_RAW_LO_TEMP 0
# endif
#endif

#ifdef THERMISTORHEATER_1
# define HEATER_1_TEMPTABLE TT_NAME(THERMISTORHEATER_1)
# define HEATER_1_TEMPTABLE_LEN (sizeof(HEATER_1_TEMPTABLE)/sizeof(*HEATER_1_TEMPTABLE))
#else
# ifdef HEATER_1_USES_THERMISTOR
#  error No heater 1 thermistor table specified
# else    # HEATER_1_USES_THERMISTOR
#  define HEATER_1_TEMPTABLE NULL
#  define HEATER_1_TEMPTABLE_LEN 0
# endif   # HEATER_1_USES_THERMISTOR
#endif

#Set the high and low raw values for the heater, this indicates which raw value is a high or low temperature
#ifndef HEATER_1_RAW_HI_TEMP
# ifdef HEATER_1_USES_THERMISTOR     #In case of a thermistor the highest temperature results in the lowest ADC value
#  define HEATER_1_RAW_HI_TEMP 0
#  define HEATER_1_RAW_LO_TEMP 16383
# else                            #In case of an thermocouple the highest temperature results in the highest ADC value
#  define HEATER_1_RAW_HI_TEMP 16383
#  define HEATER_1_RAW_LO_TEMP 0
# endif
#endif

#ifdef THERMISTORHEATER_2
# define HEATER_2_TEMPTABLE TT_NAME(THERMISTORHEATER_2)
# define HEATER_2_TEMPTABLE_LEN (sizeof(HEATER_2_TEMPTABLE)/sizeof(*HEATER_2_TEMPTABLE))
#else
# ifdef HEATER_2_USES_THERMISTOR
#  error No heater 2 thermistor table specified
# else    # HEATER_2_USES_THERMISTOR
#  define HEATER_2_TEMPTABLE NULL
#  define HEATER_2_TEMPTABLE_LEN 0
# endif   # HEATER_2_USES_THERMISTOR
#endif

#Set the high and low raw values for the heater, this indicates which raw value is a high or low temperature
#ifndef HEATER_2_RAW_HI_TEMP
# ifdef HEATER_2_USES_THERMISTOR     #In case of a thermistor the highest temperature results in the lowest ADC value
#  define HEATER_2_RAW_HI_TEMP 0
#  define HEATER_2_RAW_LO_TEMP 16383
# else                            #In case of an thermocouple the highest temperature results in the highest ADC value
#  define HEATER_2_RAW_HI_TEMP 16383
#  define HEATER_2_RAW_LO_TEMP 0
# endif
#endif

#ifdef THERMISTORBED
# define BEDTEMPTABLE TT_NAME(THERMISTORBED)
# define BEDTEMPTABLE_LEN (sizeof(BEDTEMPTABLE)/sizeof(*BEDTEMPTABLE))
#else
# ifdef BED_USES_THERMISTOR
#  error No bed thermistor table specified
# endif   # BED_USES_THERMISTOR
#endif

#Set the high and low raw values for the heater, this indicates which raw value is a high or low temperature
#ifndef HEATER_BED_RAW_HI_TEMP
# ifdef BED_USES_THERMISTOR     #In case of a thermistor the highest temperature results in the lowest ADC value
#  define HEATER_BED_RAW_HI_TEMP 0
#  define HEATER_BED_RAW_LO_TEMP 16383
# else                            #In case of an thermocouple the highest temperature results in the highest ADC value
#  define HEATER_BED_RAW_HI_TEMP 16383
#  define HEATER_BED_RAW_LO_TEMP 0
# endif
#endif

#endif   #THERMISTORTABLES_H_