from pathlib import Path
import importlib.util
import urllib.request
import xml.etree.ElementTree as ET

REPO_RAW = "https://raw.githubusercontent.com/ngmthang-g/clinent-game-than-long-DATA-2222/main"
INTERFACE_URL = REPO_RAW + "/Game/Th%E1%BA%A7n%20Long%20%20Mobile_Data/StreamingAssets/Interface.unity3d"
MATERIALIZER_URL = REPO_RAW + "/tools/materialize_tool_data.py"

root = Path("research_ui")
root.mkdir(exist_ok=True)
enc = root / "Interface.unity3d"
helper = root / "materialize_tool_data.py"
urllib.request.urlretrieve(INTERFACE_URL, enc)
urllib.request.urlretrieve(MATERIALIZER_URL, helper)

spec = importlib.util.spec_from_file_location("tlmat", helper)
mod = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mod)
dec = mod.fg_decrypt(enc.read_bytes())
print(f"decrypted_signature={dec[:16]!r} size={len(dec)}")
files = mod.extract_unityfs_bytes(dec, root / "bundle")
found = {}
for p in files:
    found.update(mod.extract_config_xml_from_cab(p, root / "xml"))
print(f"xml_assets={len(found)}")

wanted = {
    "SkillBar_Layout", "TopIcon_Layout", "QuickItemsBar_Layout", "MainUI_Layout",
    "MainUI_Features_Layout", "MainUI_OtherHeader_Layout",
}
prop_names = {
    "ClickHandler", "SelectHandler", "HoverAsClickHandler", "HoverTickHandler",
    "Sprite", "Text", "TranslationText", "Position", "Size", "Anchor", "Pivot",
}

for name in sorted(wanted):
    path = found.get(name)
    if not path:
        print(f"MISSING {name}")
        continue
    tree = ET.parse(path)
    root_el = tree.getroot()
    print("\n" + "=" * 120)
    print(f"ASSET {name}")
    print("=" * 120)
    for el in root_el.iter():
        if el.tag not in {"Button", "Toggle"}:
            continue
        record = [f"<{el.tag} Name={el.attrib.get('Name','')} Active={el.attrib.get('Active','')}>" ]
        for p in el.iter("Property"):
            pn = p.attrib.get("Name", "")
            if pn in prop_names:
                record.append(f"{pn}={p.attrib.get('Value','')}")
        print(" | ".join(record))
