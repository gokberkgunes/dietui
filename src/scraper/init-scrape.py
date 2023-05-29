#!/usr/bin/python3

from autoscraper import AutoScraper


#url = "https://tools.myfooddata.com/nutrition-facts/"
url = "https://www.diyetkolik.com/kac-kalori/muzlu-kek"
wanted_dict = {
                "name":['Muzlu Kek'],
                "kcal":['470          '],
                "carb":['42.5155984349'],
                "prot":['6.65013660836'],
                "fat" :['20.760028451 '],
                "fibr":['2.44969317776'],
                "chol":['55.2965920674'],
                "sodi":['166.624843001'],
                "pota":['236.521709684'],
                "calc":['32.4951510164'],
                "vitA":['51.1415141472'],
                "vitC":['3.77289299749'],
                "iron":['1.24777388002']
            }

scraper = AutoScraper()

result = scraper.build(url=url, wanted_dict=wanted_dict)

print(scraper.get_result_exact(url, group_by_alias=True))
scraper.save('scraper-info')
