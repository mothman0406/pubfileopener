# PUB File Opener

Open Microsoft Publisher (.pub) files in the browser, privately. Files are parsed and drawn on the user's own device; nothing is uploaded.

## How it works

- `core/` compiles [libmspub](https://wiki.documentfoundation.org/DLP/Libraries/libmspub) to WebAssembly and turns each file into a list of drawing events (JSON).
- `site/renderer.js` draws those events as one SVG per page, with text laid out by the browser.
- Older Windows graphics (WMF, EMF) and TIFF images are converted in the browser.
- `.github/workflows/build-core.yml` builds the WebAssembly core and fetches the browser libraries (pinned, checksummed) into `site/`.
- `site/` is deployed as a static site.

## Licenses

Our code is MIT. Third-party components and their licenses are listed in `THIRD_PARTY_NOTICES.md` and on the site's licenses page.
