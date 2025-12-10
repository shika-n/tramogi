#include "tramogi/core/obj_loader.h"
#include <string>

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#include "../logging.h"

namespace tramogi::core {

bool ObjLoader::load_from_file(const char *filepath) {
	std::string warn;
	std::string err;
	bool res = tinyobj::LoadObj(&attributes, &shapes, &materials, &warn, &err, filepath);
	DLOG("{}", warn + err);
	return res;
}

} // namespace tramogi::core
