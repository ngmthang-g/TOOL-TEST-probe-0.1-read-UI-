from pathlib import Path
import re
import urllib.request

import UnityPy

URL = "https://raw.githubusercontent.com/ngmthang-g/clinent-game-than-long-DATA-2222/main/Game/Th%E1%BA%A7n%20Long%20%20Mobile_Data/StreamingAssets/Interface.unity3d"
OUT = Path("research-interface.unity3d")

print(f"download {URL}")
urllib.request.urlretrieve(URL, OUT)
print(f"downloaded {OUT.stat().st_size} bytes")

env = UnityPy.load(str(OUT))
assets = []
for obj in env.objects:
    if obj.type.name != "TextAsset":
        continue
    data = obj.read()
    name = getattr(data, "m_Name", "") or getattr(data, "name", "") or ""
    raw = getattr(data, "m_Script", b"")
    if isinstance(raw, bytes):
        text = raw.decode("utf-8", errors="replace")
    else:
        text = str(raw or "")
    assets.append((name, text))

print(f"TextAssets={len(assets)}")
name_terms = ("topicon", "skillbar", "roleinfo", "mainui", "maininterface", "functionbar", "quickbar")
body_terms = re.compile(r"(?i)(bag|skill|switch|roleinfo|topicon|button|toggle|quick|expand|collapse|menu|more)")

for name, text in sorted(assets, key=lambda it: it[0].lower()):
    lname = name.lower()
    if not any(term in lname for term in name_terms):
        continue
    print("\n" + "=" * 100)
    print(f"ASSET {name} chars={len(text)}")
    print("=" * 100)
    lines = text.splitlines()
    if 1 < len(lines) <= 1200:
        matched = [line for line in lines if body_terms.search(line)]
        for line in matched[:500]:
            print(line[:4000])
        continue
    # Minified XML or one-line text: print context windows around relevant terms.
    seen = set()
    for match in body_terms.finditer(text):
        start = max(0, match.start() - 350)
        end = min(len(text), match.end() + 500)
        chunk = text[start:end].replace("\r", " ").replace("\n", " ")
        key = chunk[:160]
        if key in seen:
            continue
        seen.add(key)
        print("... " + chunk + " ...")
        if len(seen) >= 160:
            break
