#include "tramogi/core/obj_loader.h"
#include <string>

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#include "../logging.h"

namespace tramogi::core {

bool ObjLoader::load_from_file(const char *filepath) {
	tinyobj::attrib_t attributes;
	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> materials;
	std::string warn;
	std::string err;
	bool res = tinyobj::LoadObj(&attributes, &shapes, &materials, &warn, &err, filepath);
	DLOG("{}", warn + err);

	return res;
}

} // namespace tramogi::core
