from pathlib import Path
import importlib.util
import re
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

# 1) Find the exact button containing the visible Vietnamese caption Túi đồ.
print("\n=== EXACT TÚI ĐỒ BUTTON CANDIDATES ===")
for asset_name, path in sorted(found.items()):
    try:
        tree = ET.parse(path)
    except Exception:
        continue
    for el in tree.getroot().iter():
        if el.tag not in {"Button", "Toggle"}:
            continue
        values = []
        for p in el.iter("Property"):
            values.append((p.attrib.get("Name", ""), p.attrib.get("Value", "")))
        joined = " | ".join(v for _, v in values)
        if "Túi đồ" not in joined and "Tui do" not in joined:
            continue
        print(f"ASSET={asset_name} TAG={el.tag} NAME={el.attrib.get('Name','')} ACTIVE={el.attrib.get('Active','')}")
        for pn, pv in values:
            if pn in {"Text","TranslationText","Sprite","ClickHandler","SelectHandler","Position","Size","Anchor","Pivot"}:
                print(f"  {pn}={pv}")

# 2) Find all layout controls with bag-ish exact identities/handlers to cross-check.
print("\n=== BAG-NAMED BUTTONS / HANDLERS ===")
for asset_name, path in sorted(found.items()):
    try:
        tree = ET.parse(path)
    except Exception:
        continue
    for el in tree.getroot().iter():
        if el.tag not in {"Button", "Toggle"}:
            continue
        name = el.attrib.get("Name", "")
        props = {p.attrib.get("Name", ""): p.attrib.get("Value", "") for p in el.iter("Property")}
        fingerprint = "|".join([asset_name, name, props.get("ClickHandler", ""), props.get("SelectHandler", ""), props.get("Text", ""), props.get("Sprite", "")])
        if re.search(r"(?i)(bag|túi đồ|tui do)", fingerprint):
            print(f"ASSET={asset_name} TAG={el.tag} NAME={name} HANDLER={props.get('ClickHandler') or props.get('SelectHandler','')} SPRITE={props.get('Sprite','')} TEXT={props.get('Text','')}")

# 3) Print the exact SkillBar switch control from XML.
print("\n=== SKILLBAR SWITCH CONTROL ===")
skill = found.get("SkillBar_Layout")
if skill:
    tree = ET.parse(skill)
    for el in tree.getroot().iter():
        if el.attrib.get("Name") == "ButtonOriginalSwitchSite":
            print(ET.tostring(el, encoding="unicode")[:12000])

# 4) Recover Lua source context straight from the CAB byte stream. Lua TextAsset strings are
# stored contiguously even though the XML helper only materializes XML TextAssets.
needle = b"ButtonOriginalSwitchSiteClicked"
print("\n=== LUA CONTEXT: ButtonOriginalSwitchSiteClicked ===")
for cab in files:
    data = cab.read_bytes()
    start = 0
    while True:
        idx = data.find(needle, start)
        if idx < 0:
            break
        lo = max(0, idx - 12000)
        hi = min(len(data), idx + 24000)
        chunk = data[lo:hi].decode("utf-8", errors="ignore")
        # Prefer a tight function block when recognizable.
        m = re.search(r"function\s+SkillBar:ButtonOriginalSwitchSiteClicked\s*\([^)]*\)(.*?)(?=\nfunction\s+SkillBar:|\Z)", chunk, re.S)
        if m:
            print("function SkillBar:ButtonOriginalSwitchSiteClicked(...)" + m.group(1)[:14000])
        else:
            pos = chunk.find("ButtonOriginalSwitchSiteClicked")
            print(chunk[max(0,pos-5000):pos+12000])
        start = idx + len(needle)

# 5) Also inspect nearby switch-state identifiers globally in SkillBar Lua context.
print("\n=== LUA CONTEXT: SkillBar switch-state keywords ===")
for cab in files:
    data = cab.read_bytes()
    for keyword in [b"OriginalSwitchSite", b"SkillTab", b"QuickItemsBar", b"ButtonOriginalSwitchSite"]:
        idx = data.find(keyword)
        if idx >= 0:
            chunk = data[max(0, idx-5000):min(len(data), idx+9000)].decode("utf-8", errors="ignore")
            print(f"--- keyword={keyword.decode()} ---")
            print(chunk[:14000])
