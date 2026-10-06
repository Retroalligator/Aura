"""Create Finder layout metadata on a mounted writable image (no UI automation)."""
from pathlib import Path
import sys
from ds_store import DSStore
from mac_alias import Alias

volume = Path(sys.argv[1])
background = Alias.for_file(str(volume / ".background/background.png")).to_bytes()
with DSStore.open(str(volume / ".DS_Store"), "w+") as store:
    store["."]["vSrn"] = ("long", 1)
    store["."]["icvl"] = ("type", b"icnv")
    store["."]["bwsp"] = {
        "ShowStatusBar": False, "ShowToolbar": False, "ShowSidebar": False,
        "ShowPathbar": False, "ShowTabView": False,
        "WindowBounds": "{{160, 120}, {760, 460}}",
    }
    store["."]["icvp"] = {
        "viewOptionsVersion": 1, "backgroundType": 2,
        "backgroundImageAlias": background,
        "iconSize": 72.0, "textSize": 12.0, "gridSpacing": 100.0,
        "gridOffsetX": 0.0, "gridOffsetY": 0.0, "arrangeBy": "none",
        "labelOnBottom": True, "showIconPreview": True,
    }
    for name, position in {
        "Aura.vst3": (170, 145), "Install VST3 Here": (550, 145),
        "Aura.component": (170, 285), "Install AU Here": (550, 285),
        "INSTALL.txt": (340, 370), "Aura — Installation Guide.svg": (500, 370),
    }.items():
        store[name]["Iloc"] = position
