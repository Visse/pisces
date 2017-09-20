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

            if (!data->objects.empty()) {
                auto &back = data->objects.back();
                back.indexCount = (int)data->faces.size() - back.firstIndex;
            }

            Object object;
            object.name = Common::CreateStringId(name);
            object.firstIndex = (int)data->faces.size();
            data->objects.push_back(object);
        };


        Data data;
        data.filename = filename.c_str();
        std::string error;
        if (!tinyobj::LoadObjWithCallback(file, callbacks, &data, nullptr, &error)) {
            THROW(std::runtime_error, "Failed to load obj file \"%s\" error: %s", filename.c_str(), error.c_str());   
        }
        
        if (!data.objects.empty()) {
            auto &back = data.objects.back();
            back.indexCount = (int)data.faces.size() - back.firstIndex;
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

        for (auto iter=data.objects.begin(), end=data.objects.end(); iter != end; ++iter) {
            Object object;
            object.firstIndex = result.indexes.size();
            object.firstVertex = result.vertexes.size();

            object.name = iter->name;
            // No sharing of indexes between objects
            indexLookup.clear();

            for (int i=0, c=iter->indexCount; i < c; ++i) {
                Face face = data.faces[i + iter->firstIndex];
                for (int i=0; i < 3; ++i) {
                    index_t index = face.indexes[i];

                    int idx = lookupIndex(index);
                    if (idx == -1) {
                        Vertex vertex;
                        vertex.pos[0] = data.positions[index.vertex_index-1].x;
                        vertex.pos[1] = data.positions[index.vertex_index-1].y;
                        vertex.pos[2] = data.positions[index.vertex_index-1].z;
                        vertex.uv[0] = glm::packUnorm1x16(data.texcoords[index.texcoord_index-1].x);
                        vertex.uv[1] = glm::packUnorm1x16(data.texcoords[index.texcoord_index-1].y);
                        vertex.normal = glm::packSnorm3x10_1x2(glm::vec4(data.normals[index.normal_index-1], 0.f));

                        result.vertexes.push_back(vertex);
                        idx = (int)result.vertexes.size() - 1;

                        indexLookup[index] = idx;
                        object.vertexCount++;
                    }

                    object.indexCount++;
                    result.indexes.push_back(idx);
                }
            }

            result.objects.push_back(object);
        }

        return std::move(result);
    }

}