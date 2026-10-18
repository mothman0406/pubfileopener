#!/bin/bash
# Builds site/core/pubcore.{js,wasm} from pinned upstream sources and fetches pinned browser libraries.
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd); W=$ROOT/.build; mkdir -p "$W"; cd "$W"
EMSDK_VERSION=$(cat "$ROOT/core/EMSDK_VERSION")
fetch() { curl -fsSL --retry 3 -o "$1" "$2"; echo "$3  $1" | sha256sum -c -; }
fetch libmspub-0.1.4.tar.xz https://dev-www.libreoffice.org/src/libmspub-0.1.4.tar.xz ef36c1a1aabb2ba3b0bedaaafe717bf4480be2ba8de6f3894be5fd3702b013ba
fetch librevenge-0.0.5.tar.bz2 https://dev-www.libreoffice.org/src/librevenge-0.0.5.tar.bz2 5892ca6796f7a2a93d580832e907e849b19d980b40d326a283b18877ab6de0c5
tar xf libmspub-0.1.4.tar.xz && tar xf librevenge-0.0.5.tar.bz2
(cd librevenge-0.0.5 && ./configure --disable-tests --disable-werror --without-docs > /dev/null)
(cd libmspub-0.1.4 && ./configure --disable-werror --without-docs > /dev/null)
[ -d emsdk ] || git clone --depth 1 https://github.com/emscripten-core/emsdk.git
./emsdk/emsdk install "$EMSDK_VERSION" > /dev/null && ./emsdk/emsdk activate "$EMSDK_VERSION" > /dev/null
source ./emsdk/emsdk_env.sh > /dev/null
mkdir -p icu boost obj && cp -r /usr/include/unicode icu/ && ln -sfn /usr/include/boost boost/boost
RV=$W/librevenge-0.0.5; MP=$W/libmspub-0.1.4; CF="-O2 -std=c++17 -DHAVE_CONFIG_H -DNDEBUG -fwasm-exceptions -sUSE_ZLIB=1 -I$W/boost"
for f in "$RV"/src/lib/*.cpp; do
  case $(basename "$f") in *Generator*|RVNGHTML*|RVNGCSV*|RVNGRaw*|RVNGSVG*) continue;; esac
  em++ $CF -I"$RV" -I"$RV/inc" -I"$RV/src/lib" -c "$f" -o "obj/rv_$(basename "$f" .cpp).o"
done
for f in "$MP"/src/lib/*.cpp; do em++ $CF -I"$MP" -I"$MP/inc" -I"$MP/src/lib" -I"$RV/inc" -I"$W/icu" -c "$f" -o "obj/mp_$(basename "$f" .cpp).o"; done
em++ $CF -I"$W/icu" -c "$ROOT/core/icu_shim.cpp" -o obj/zz_icu.o
mkdir -p "$ROOT/site/core"
em++ $CF -I"$RV/inc" -I"$MP/inc" "$ROOT/core/pubjson_web.cpp" obj/*.o -Os -o "$ROOT/site/core/pubcore.js" \
  -sMODULARIZE=1 -sEXPORT_NAME=PubCore -sENVIRONMENT=web,worker -sALLOW_MEMORY_GROWTH=1 \
  -sEXPORTED_FUNCTIONS=_pub_to_json,_pub_free,_malloc,_free -sEXPORTED_RUNTIME_METHODS=UTF8ToString,getValue,HEAPU8
mkdir -p "$ROOT/site/lib"; cd "$ROOT/site/lib"
fetch pako.min.js https://cdn.jsdelivr.net/npm/pako@2.1.0/dist/pako.min.js ede2693a4a6a5126b9d35669062b358ecab6ae7b9b86a1cf302feb45a8514907
fetch UTIF.js https://cdn.jsdelivr.net/npm/utif@3.1.0/UTIF.js e3e76115f49571e39624c3316a76b3c4c5b2c5ca518dfec4b66a9f7af8c6d059
fetch WMFJS.bundle.min.js https://cdn.jsdelivr.net/npm/rtf.js@3.0.9/dist/WMFJS.bundle.min.js 663bd60fc04e8983fd7681b8600fe365eeb835e6a26b9f1dc50cf5bee3df9c8e
fetch EMFJS.bundle.min.js https://cdn.jsdelivr.net/npm/rtf.js@3.0.9/dist/EMFJS.bundle.min.js 61c06634290802d6f44976603a068161b2d129a34398d9e187358693df8f12bb
echo "built: $(ls -la "$ROOT/site/core")"
