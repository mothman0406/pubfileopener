// Browser entry point: parse a Publisher file held in memory and return its drawing events as JSON.
#define PUB_NO_MAIN
#include "pubjson.cpp"
#include <cstdlib>
extern "C" {
// status: 0 parsed, 3 not a Publisher file, 4 parse failed (partial JSON may still be returned).
char *pub_to_json(const unsigned char *data, unsigned len, int *status)
{
  librevenge::RVNGStringStream input(data, len);
  if (!libmspub::MSPUBDocument::isSupported(&input)) { if (status) *status = 3; return nullptr; }
  input.seek(0, librevenge::RVNG_SEEK_SET);
  JsonGen g;
  g.out = "[";
  bool ok = libmspub::MSPUBDocument::parse(&input, &g);
  g.out += "\n]\n";
  if (status) *status = ok ? 0 : 4;
  char *r = static_cast<char *>(std::malloc(g.out.size() + 1));
  if (r) std::memcpy(r, g.out.c_str(), g.out.size() + 1);
  return r;
}
void pub_free(void *p) { std::free(p); }
}
