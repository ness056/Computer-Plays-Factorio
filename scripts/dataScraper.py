## This script is used to scrap Factorio data.raw table to list in
## the C++ program all the prototypes of entities, items, etc.
##
## Note: the path to a Factorio binary must be specified in the args
## when running this script.


import sys
import subprocess
import pathlib
import json

if __name__ == "__main__":
    if len(sys.argv) < 1:
        print("Error: you must specify the path to factorio's binary.")

    factorioPath = sys.argv[1]
    mods = (pathlib.Path(__file__) / "../../data/dataScraper").resolve()

    args = [
        factorioPath,
        "--mod-directory", str(mods),
        "--start-server-load-scenario", "base/freeplay",
    ]

    out = subprocess.run(args, capture_output=True).stdout

    sizeStart = out.find(bytes("DATA", "UTF-8")) + 4
    sizeEnd = out.find(bytes(" ", "UTF-8"), sizeStart)
    dataStart = sizeEnd + 1
    dataEnd = dataStart + int(out[sizeStart:sizeEnd])

    print(len(out), sizeStart, dataEnd - dataStart, sizeStart + dataEnd - dataStart)
    print(out[dataEnd-100:dataEnd])
    print(out[dataStart-2:dataStart+100])
    data = json.loads(out[dataStart:dataEnd])
    
    cppPath = (pathlib.Path(__file__) / "../../src/factorioData.hpp").resolve()
    file = open(cppPath, "w+", encoding="UTF-8")
    file.write("// This file was automatically generated with scripts/dataScraper.py\n")
    file.write("// It contains some useful data from the Factorio prototypes table.\n\n")

    file.write("namespace ComputerPlaysFactorio {")

    file.write("enum FactorioEntity {")
    for k in data.keys():
        print(k)
    file.write("};")

    file.write("}") #namespace
    file.close()