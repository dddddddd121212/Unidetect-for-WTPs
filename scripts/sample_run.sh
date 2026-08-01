#!/bin/bash
ScriptPath="$( cd "$(dirname "$BASH_SOURCE")" ; pwd -P )"

while true; do
echo "[INFO] The sample starts to run"
running_command="./main ../scripts/test.json"
cd ${ScriptPath}/../out
${running_command}
if [ $? -ne 0 ];then
   		echo "[INFO]  The program runs failed. Restarting..."
else
   		echo "[INFO] The program runs successfully"
fi
sleep 2
done