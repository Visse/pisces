#include "ObjLoader.h"

#include "Common/Throw.h"

#include "tiny_obj_loader.h"

#include <fstream>

#include <glm/vec3.hpp>
#include <glm/vec2.hpp>

#include <glm/gtc/packing.hpp>

namespace Pisces
{
    PISCES_API ObjLoader::Result ObjLoader::LoadFile( const std::string &filename )
    {
        using namespace tinyobj;

        std::ifstream file(filename);

        struct Face {
            index_t indexes[3];
        };

        struct Data {
            const char *filename;
            std::vector<glm::vec3> positions;
            std::vector<glm::vec3> normals;
            std::vector<glm::vec2> texcoords;
            std::vector<Face> faces;
            std::vector<Object> objects;
        };

        tinyobj::callback_t callbacks;

        callbacks.vertex_cb = [](void *user_data, real_t x, real_t y, real_t z, real_t w) {
            Data *data = (Data*)user_data;
            data->positions.emplace_back(x, y, z);
        };
        callbacks.normal_cb = [](void *user_data, real_t x, real_t y, real_t z) {
            Data *data = (Data*)user_data;
            data->normals.emplace_back(x, y, z);
        };
        callbacks.texcoord_cb = [](void *user_data, real_t x, real_t y, real_t z) {
            Data *data = (Data*)user_data;
            data->texcoords.emplace_back(x,y);
        };
        callbacks.index_cb = [](void *user_data, index_t *indices, int num_indices) {
            Data *data = (Data*)user_data;

            if (num_indices != 3) {
                THROW(std::runtime_error, "Failed to load obj file \"%s\" unsuported! (only support triangles this file contains %i-gons", data->filename, num_indices);
            }

            Face face;
                face.indexes[0] = indices[0];
                face.indexes[1] = indices[1];
                face.indexes[2] = indices[2];

            data->faces.emplace_back(face);
        };
        callbacks.usemtl_cb = [](void *user_data, const char *name, int material_id) {};
        callbacks.mtllib_cb = [](void *user_data, const material_t *materials, int num_materials) {};

        callbacks.group_cb = [](void *user_data, const char **names, int num_names) {};
        callbacks.object_cb = [](void *user_data, const char *name) {
            Data *data = (Data*)user_data;

            Object object;
            object.name = Common::CreateStringId(name);
            object.first = (int)data->faces.size();
            data->objects.push_back(object);
        };


        Data data;
        data.filename = filename.c_str();
        std::string error;
        if (!tinyobj::LoadObjWithCallback(file, callbacks, &data, nullptr, &error)) {
            THROW(std::runtime_error, "Failed to load obj file \"%s\" error: %s", filename.c_str(), error.c_str());   
        }


        struct Less {
            bool operator () ( const index_t &lhs, const index_t &rhs) const {
                return std::tie(lhs.vertex_index, lhs.normal_index, lhs.texcoord_index) <
                       std::tie(rhs.vertex_index, rhs.normal_index, rhs.texcoord_index);
            };
        };
        std::map<index_t, int, Less> indexLookup;

        auto lookupIndex = [&]( index_t index ) {
            auto iter = indexLookup.find(index);
            if (iter != indexLookup.end()) {
                return iter->second;
            }
            return -1;
        };

        Result result;

        for (Face face : data.faces) {
            for (int i=0; i < 3; ++i) {
                index_t index = face.indexes[i];

                int idx = lookupIndex(index);
                if (idx == -1) {
                    Vertex vertex;
                    vertex.x = data.positions[idx].x;
                    vertex.y = data.positions[idx].y;
                    vertex.z = data.positions[idx].z;
                    vertex.uvx = glm::packUnorm1x16(data.texcoords[idx].x);
                    vertex.uvy = glm::packUnorm1x16(data.texcoords[idx].y);
                    vertex.normal = glm::packSnorm3x10_1x2(glm::vec4(data.normals[idx], 0.f));

                    result.vertexes.push_back(vertex);
                    idx = (int)result.vertexes.size() - 1;

                    indexLookup[index] = idx;
                }

                result.indexes.push_back(idx);
            }
        }

        result.objects = std::move(data.objects);
        
        if (!result.objects.empty()) {
            result.objects[0].first *= 3;
        }
        int last = 0;
        for (int i=1, e =(int)result.objects.size(); i < e; ++i) {
            result.objects[i].first *= 3;
            result.objects[i-1].count = result.objects[i].first - result.objects[i-1].first;
            last = result.objects[i].first;
        }
        
        if (!result.objects.empty()) {
            result.objects.back().count = result.objects.back().first - last;
        }

        return std::move(result);
    }

}