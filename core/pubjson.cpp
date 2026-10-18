// pubjson: turn a Microsoft Publisher file into a JSON list of drawing events via libmspub.
// Used natively for testing and compiled to WebAssembly for the browser app.
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cctype>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <librevenge/librevenge.h>
#include <librevenge-stream/librevenge-stream.h>
#include <libmspub/libmspub.h>

static void jsonEsc(std::string &o, const char *s)
{
  o += '"';
  for (const unsigned char *p = reinterpret_cast<const unsigned char *>(s); p && *p; ++p)
  {
    unsigned char c = *p;
    if (c == '"') o += "\\\"";
    else if (c == '\\') o += "\\\\";
    else if (c == '\n') o += "\\n";
    else if (c == '\r') o += "\\r";
    else if (c == '\t') o += "\\t";
    else if (c < 0x20) { char b[8]; std::snprintf(b, sizeof b, "\\u%04x", c); o += b; }
    else o += static_cast<char>(c);
  }
  o += '"';
}
static void writeProps(std::string &o, const librevenge::RVNGPropertyList &pl);
static void writeVec(std::string &o, const librevenge::RVNGPropertyListVector &v)
{
  o += '[';
  for (unsigned long k = 0; k < v.count(); ++k) { if (k) o += ','; writeProps(o, v[k]); }
  o += ']';
}
static void writeProps(std::string &o, const librevenge::RVNGPropertyList &pl)
{
  o += '{';
  bool first = true;
  librevenge::RVNGPropertyList::Iter i(pl);
  for (i.rewind(); i.next();)
  {
    if (!first) o += ',';
    first = false;
    jsonEsc(o, i.key());
    o += ':';
    if (i.child()) writeVec(o, *i.child());
    else if (i()) { librevenge::RVNGString s = i()->getStr(); jsonEsc(o, s.cstr()); }
    else o += "null";
  }
  o += '}';
}

class JsonGen : public librevenge::RVNGDrawingInterface
{
public:
  std::string out;
  bool first = true;
  void ev(const char *t, const librevenge::RVNGPropertyList *pl = nullptr)
  {
    out += first ? "\n" : ",\n";
    first = false;
    out += "[\""; out += t; out += "\"";
    if (pl) { out += ','; writeProps(out, *pl); }
    out += ']';
  }
  void evText(const char *t, const librevenge::RVNGString &s)
  {
    out += first ? "\n" : ",\n";
    first = false;
    out += "[\""; out += t; out += "\",";
    jsonEsc(out, s.cstr());
    out += ']';
  }
  void startDocument(const librevenge::RVNGPropertyList &p) override { ev("startDocument", &p); }
  void endDocument() override { ev("endDocument"); }
  void setDocumentMetaData(const librevenge::RVNGPropertyList &p) override { ev("setDocumentMetaData", &p); }
  void defineEmbeddedFont(const librevenge::RVNGPropertyList &p) override { ev("defineEmbeddedFont", &p); }
  void startPage(const librevenge::RVNGPropertyList &p) override { ev("startPage", &p); }
  void endPage() override { ev("endPage"); }
  void startMasterPage(const librevenge::RVNGPropertyList &p) override { ev("startMasterPage", &p); }
  void endMasterPage() override { ev("endMasterPage"); }
  void setStyle(const librevenge::RVNGPropertyList &p) override { ev("setStyle", &p); }
  void startLayer(const librevenge::RVNGPropertyList &p) override { ev("startLayer", &p); }
  void endLayer() override { ev("endLayer"); }
  void startEmbeddedGraphics(const librevenge::RVNGPropertyList &p) override { ev("startEmbeddedGraphics", &p); }
  void endEmbeddedGraphics() override { ev("endEmbeddedGraphics"); }
  void openGroup(const librevenge::RVNGPropertyList &p) override { ev("openGroup", &p); }
  void closeGroup() override { ev("closeGroup"); }
  void drawRectangle(const librevenge::RVNGPropertyList &p) override { ev("drawRectangle", &p); }
  void drawEllipse(const librevenge::RVNGPropertyList &p) override { ev("drawEllipse", &p); }
  void drawPolygon(const librevenge::RVNGPropertyList &p) override { ev("drawPolygon", &p); }
  void drawPolyline(const librevenge::RVNGPropertyList &p) override { ev("drawPolyline", &p); }
  void drawPath(const librevenge::RVNGPropertyList &p) override { ev("drawPath", &p); }
  void drawGraphicObject(const librevenge::RVNGPropertyList &p) override { ev("drawGraphicObject", &p); }
  void drawConnector(const librevenge::RVNGPropertyList &p) override { ev("drawConnector", &p); }
  void startTextObject(const librevenge::RVNGPropertyList &p) override { ev("startTextObject", &p); }
  void endTextObject() override { ev("endTextObject"); }
  void startTableObject(const librevenge::RVNGPropertyList &p) override { ev("startTableObject", &p); }
  void openTableRow(const librevenge::RVNGPropertyList &p) override { ev("openTableRow", &p); }
  void closeTableRow() override { ev("closeTableRow"); }
  void openTableCell(const librevenge::RVNGPropertyList &p) override { ev("openTableCell", &p); }
  void closeTableCell() override { ev("closeTableCell"); }
  void insertCoveredTableCell(const librevenge::RVNGPropertyList &p) override { ev("insertCoveredTableCell", &p); }
  void endTableObject() override { ev("endTableObject"); }
  void insertTab() override { ev("insertTab"); }
  void insertSpace() override { ev("insertSpace"); }
  void insertText(const librevenge::RVNGString &s) override { evText("insertText", s); }
  void insertLineBreak() override { ev("insertLineBreak"); }
  void insertField(const librevenge::RVNGPropertyList &p) override { ev("insertField", &p); }
  void openOrderedListLevel(const librevenge::RVNGPropertyList &p) override { ev("openOrderedListLevel", &p); }
  void openUnorderedListLevel(const librevenge::RVNGPropertyList &p) override { ev("openUnorderedListLevel", &p); }
  void closeOrderedListLevel() override { ev("closeOrderedListLevel"); }
  void closeUnorderedListLevel() override { ev("closeUnorderedListLevel"); }
  void openListElement(const librevenge::RVNGPropertyList &p) override { ev("openListElement", &p); }
  void closeListElement() override { ev("closeListElement"); }
  void defineParagraphStyle(const librevenge::RVNGPropertyList &p) override { ev("defineParagraphStyle", &p); }
  void openParagraph(const librevenge::RVNGPropertyList &p) override { ev("openParagraph", &p); }
  void closeParagraph() override { ev("closeParagraph"); }
  void defineCharacterStyle(const librevenge::RVNGPropertyList &p) override { ev("defineCharacterStyle", &p); }
  void openSpan(const librevenge::RVNGPropertyList &p) override { ev("openSpan", &p); }
  void closeSpan() override { ev("closeSpan"); }
  void openLink(const librevenge::RVNGPropertyList &p) override { ev("openLink", &p); }
  void closeLink() override { ev("closeLink"); }
};

// A structured stream fed from streams extracted by another container reader.
class MapStream : public librevenge::RVNGInputStream
{
  std::vector<std::string> m_names;
  std::map<std::string, std::vector<unsigned char>> m_data;
  static std::string lower(const char *s) { std::string r(s ? s : ""); for (auto &c : r) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c))); return r; }
public:
  void add(const std::string &name, std::vector<unsigned char> d) { m_names.push_back(name); m_data[lower(name.c_str())] = std::move(d); }
  static MapStream *fromBundle(const char *path)
  {
    FILE *f = std::fopen(path, "rb");
    if (!f) return nullptr;
    MapStream *m = new MapStream();
    auto rd32 = [&](uint32_t &v) { unsigned char b[4]; if (std::fread(b, 1, 4, f) != 4) return false; v = b[0] | (b[1] << 8) | (b[2] << 16) | (uint32_t(b[3]) << 24); return true; };
    uint32_t nl, dl;
    while (rd32(nl))
    {
      std::string name(nl, '\0');
      if (std::fread(&name[0], 1, nl, f) != nl || !rd32(dl)) break;
      std::vector<unsigned char> d(dl);
      if (dl && std::fread(d.data(), 1, dl, f) != dl) break;
      m->add(name, std::move(d));
    }
    std::fclose(f);
    return m;
  }
  bool isStructured() override { return true; }
  unsigned subStreamCount() override { return static_cast<unsigned>(m_names.size()); }
  const char *subStreamName(unsigned id) override { return id < m_names.size() ? m_names[id].c_str() : nullptr; }
  bool existsSubStream(const char *name) override { return name && m_data.count(lower(name)); }
  librevenge::RVNGInputStream *getSubStreamByName(const char *name) override
  {
    auto it = m_data.find(lower(name));
    if (it == m_data.end() || it->second.empty()) return nullptr;
    return new librevenge::RVNGStringStream(it->second.data(), static_cast<unsigned>(it->second.size()));
  }
  librevenge::RVNGInputStream *getSubStreamById(unsigned id) override { return id < m_names.size() ? getSubStreamByName(m_names[id].c_str()) : nullptr; }
  const unsigned char *read(unsigned long, unsigned long &got) override { got = 0; return nullptr; }
  int seek(long, librevenge::RVNG_SEEK_TYPE) override { return 0; }
  long tell() override { return 0; }
  bool isEnd() override { return true; }
};

#ifndef PUB_NO_MAIN
int main(int argc, char **argv)
{
  if (argc < 3) { std::fprintf(stderr, "usage: pubjson in.pub out.json [--bundle]\n"); return 2; }
  bool bundle = argc > 3 && std::string(argv[3]) == "--bundle";
  std::unique_ptr<librevenge::RVNGInputStream> input;
  if (bundle) input.reset(MapStream::fromBundle(argv[1]));
  else input.reset(new librevenge::RVNGFileStream(argv[1]));
  if (!input) { std::printf("BAD_INPUT\n"); return 5; }
  if (!libmspub::MSPUBDocument::isSupported(input.get())) { std::printf("UNSUPPORTED\n"); return 3; }
  input->seek(0, librevenge::RVNG_SEEK_SET);
  JsonGen g;
  g.out = "[";
  bool ok = libmspub::MSPUBDocument::parse(input.get(), &g);
  g.out += "\n]\n";
  FILE *f = std::fopen(argv[2], "wb");
  if (!f) return 6;
  std::fwrite(g.out.data(), 1, g.out.size(), f);
  std::fclose(f);
  std::printf("%s %zu\n", ok ? "OK" : "PARSE_FAILED", g.out.size());
  return ok ? 0 : 4;
}
#endif
