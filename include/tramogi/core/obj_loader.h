#pragma once

#include <tiny_obj_loader.h>
#include <vector>

namespace tramogi::core {

using Attr = tinyobj::attrib_t;
using Shape = tinyobj::shape_t;
using ObjMaterial = tinyobj::material_t;

class ObjLoader {
public:
	bool load_from_file(const char *filepath);

	const Attr &get_attributes() const {
		return attributes;
	}
	const std::vector<Shape> &get_shapes() const {
		return shapes;
	}

private:
	Attr attributes;
	std::vector<Shape> shapes;
	std::vector<ObjMaterial> materials;
};

} // namespace tramogi::core
