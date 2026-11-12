#!/bin/bash
# PortMaster 啟動檔（ArkOS、ROCKNIX、muOS 等）：把本檔與 qinport/ 資料夾放進 ports/，
# 再把自己的正版遊戲資料夾複製成 qinport/game/
XDG_DATA_HOME=${XDG_DATA_HOME:-$HOME/.local/share}
if [ -d "/opt/system/Tools/PortMaster/" ]; then
  controlfolder="/opt/system/Tools/PortMaster"
elif [ -d "/opt/tools/PortMaster/" ]; then
  controlfolder="/opt/tools/PortMaster"
elif [ -d "$XDG_DATA_HOME/PortMaster/" ]; then
  controlfolder="$XDG_DATA_HOME/PortMaster"
else
  controlfolder="/roms/ports/PortMaster"
fi
source $controlfolder/control.txt
[ -f "${controlfolder}/mod_${CFW_NAME}.txt" ] && source "${controlfolder}/mod_${CFW_NAME}.txt"
get_controls

GAMEDIR="/$directory/ports/qinport"
cd "$GAMEDIR"
export SDL_GAMECONTROLLERCONFIG="$sdl_controllerconfig"
$ESUDO chmod +x ./qin
# 離開：Select + Start（引擎自己處理，不需要 gptokeyb）
./qin game xianyang --640 > log.txt 2>&1
pm_finish
