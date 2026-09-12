from pathlib import Path
import importlib.util
import re
import urllib.request

REPO_RAW = "https://raw.githubusercontent.com/ngmthang-g/clinent-game-than-long-DATA-2222/main"
INTERFACE_URL = REPO_RAW + "/Game/Th%E1%BA%A7n%20Long%20%20Mobile_Data/StreamingAssets/Interface.unity3d"
MATERIALIZER_URL = REPO_RAW + "/tools/materialize_tool_data.py"

root = Path("research_ui")
root.mkdir(exist_ok=True)
enc = root / "Interface.unity3d"
helper = root / "materialize_tool_data.py"

urllib.request.urlretrieve(INTERFACE_URL, enc)
urllib.request.urlretrieve(MATERIALIZER_URL, helper)
print(f"encrypted={enc.stat().st_size}")

spec = importlib.util.spec_from_file_location("tlmat", helper)
mod = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mod)

dec = mod.fg_decrypt(enc.read_bytes())
print(f"decrypted_signature={dec[:16]!r} size={len(dec)}")

bundle_dir = root / "bundle"
files = mod.extract_unityfs_bytes(dec, bundle_dir)
print(f"unityfs_entries={len(files)}")

xml_dir = root / "xml"
found = {}
for p in files:
    try:
        got = mod.extract_config_xml_from_cab(p, xml_dir)
        found.update(got)
    except Exception as exc:
        print(f"xml scan skipped {p.name}: {exc}")
print(f"xml_assets={len(found)}")

name_terms = ("topicon", "skillbar", "roleinfo", "mainui", "maininterface", "functionbar", "quickbar", "shortcut", "operation")
body_terms = re.compile(r"(?i)(bag|skill|switch|roleinfo|topicon|button|toggle|quick|expand|collapse|menu|more|shortcut|operation|function)")

for name, path in sorted(found.items(), key=lambda it: it[0].lower()):
    lname = name.lower()
    if not any(term in lname for term in name_terms):
        continue
    text = path.read_text(encoding="utf-8", errors="replace")
    print("\n" + "=" * 100)
    print(f"ASSET {name} chars={len(text)}")
    print("=" * 100)
    # Layout XML is usually compact; emit each element/tag fragment that contains target words.
    chunks = re.split(r"(?=<)|(?<=>)", text)
    matched = []
    for chunk in chunks:
        if body_terms.search(chunk):
            clean = " ".join(chunk.split())
            if clean and clean not in matched:
                matched.append(clean)
    if matched:
        for line in matched[:500]:
            print(line[:5000])
    else:
        # Fallback context windows for minified/odd XML.
        seen = set()
        for match in body_terms.finditer(text):
            start = max(0, match.start() - 500)
            end = min(len(text), match.end() + 800)
            chunk = " ".join(text[start:end].split())
            if chunk in seen:
                continue
            seen.add(chunk)
            print("... " + chunk[:6000] + " ...")
            if len(seen) >= 100:
                break
