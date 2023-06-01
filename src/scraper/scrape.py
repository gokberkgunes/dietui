#!/usr/bin/python3

from autoscraper import AutoScraper

scraper = AutoScraper()

scraper.load('./scraper-info')

def getfooddata(url):
    return scraper.get_result_exact(url, group_by_alias=True)

def turkish2ascii(string):
    charmap = {
        'ı': 'i', 'ğ': 'g', 'ü': 'u', 'ö': 'o', 'ş': 's', 'ç': 'c',
        'İ': 'I', 'Ğ': 'G', 'Ü': 'U', 'Ö': 'O', 'Ş': 'S', 'Ç': 'C' }

    asciistr = ""
    for char in string:
        newchar = charmap.get(char,char)
        if ord(newchar) < 128: # Only if character is in ascii, add.
            asciistr += newchar
    return asciistr


def stepwise_round(num):
    num = float(num)
    if (num is None):
        return None
    elif (num >= 100):
        return round(num, 0)
    elif (num >= 10):
        return round(num, 1)
    elif (num >= 1):
        return round(num, 2)
    elif (num < 1):
        return round(num, 3)

# A & B  are done scanning on https://www.diyetkolik.com/kac-kalori/arama/b?p=9
foodnames = [
            'ananas',
            'antep-fistigi-butun',
            'armut',
            'asure',
            'avokado',
            'ay-cekirdegi',
            'ayran',
            'ayva',
            'balkabagi',
            'beyti-kebabi',
            'boza',
            'baklava',
            'baileys',
            'barbun',
            'beyaz-lahana',
            'beyaz-peynir',
            'beyaz-uzum',
            'biber-tursusu',
            'bira',
            'bitter-cikolata',
            'bogurtlen',
            'brokoli',
            'bruksel-lahanasi',
            'bulgur-pilavi',
            'ceviz',
            'cevizli-baklava',
            'cilek',
            'fistik',
            'greyfurt',
            'haslanmis-patates',
            'hindi-salam',
            'kavun',
            'kivi',
            'limon',
            'makarna',
            'marul',
            'muz',
            'pekmez',
            'portakal',
            'sekersiz-filtre-kahve',
            'semizotu',
            'tereyagli-pirinc-pilavi',
            'yumurta',
            ]

broken_names = [
            'ahududu',
            'badem',
            'biber',
            'cay',
            'sogan',
            'domates'
            'lahana',
            'ispanak',
            'findik',
            ]


print("name,kcal,carb(g),prot(g),fat(g),fibr(g),chol(mg),sodi(mg),pota(mg),calc(mg),vitA(iu),vitC(mg),iron(mg)")
for name in foodnames:
    url = f"https://www.diyetkolik.com/kac-kalori/{name}"
    res = getfooddata(url)

    # Insert the name after substituting the Turkish letters
    data = [ turkish2ascii(list(res.values())[0][0]), 0 ]

    # Insert the data into an array after rounding them.
    for val in list(res.values())[1:]:
        tmp = stepwise_round(val[0])
        data.append(tmp)

    # Calculate kcal per 100g
    data[1] = int((data[2]+data[3])*4 + data[4]*9)

    print(*data, sep=',')
