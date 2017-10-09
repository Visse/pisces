#include "ObjLoader.h"

#include "Common/Throw.h"

#include "tiny_obj_loader.h"
#include "Common/Throw.h"
#include "Common/MemStreamBuf.h"

#include <glm/vec3.hpp>
#include <glm/vec2.hpp>

#include <glm/gtc/packing.hpp>


namespace Pisces
{
    PISCES_API ObjLoader::Result ObjLoader::LoadFile( Common::Archive &archive, const std::string &filename )
    {
        using namespace tinyobj;

        auto file = archive.openFile(filename);
        if (!file) {
            THROW(std::runtime_error, 
                  "Failed to open file \"%s\" in archive \"%s\"", filename.c_str(), archive.name()
            );
        }

        Common::MemStreamBuf buf(archive.mapFile(file), archive.fileSize(file));
        std::istream stream(&buf);


        struct Face {
            Common::StringId material;
            index_t indexes[3];
        };
        struct ObjectData {
            Common::StringId name;
            size_t firstFace = 0, faceCount = 0;
        };
        struct Data {
            const char *filename;
            std::vector<glm::vec3> positions;
            std::vector<glm::vec3> normals;
            std::vector<glm::vec2> texcoords;
            std::vector<Face> faces;
            std::vector<ObjectData> objects;
            Common::StringId material;
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
                face.material = data->material;
                face.indexes[0] = indices[0];
                face.indexes[1] = indices[1];
                face.indexes[2] = indices[2];

            data->faces.emplace_back(face);
            data->objects.back().faceCount++;
        };
        callbacks.usemtl_cb = [](void *user_data, const char *name, int material_id) {
            Data *data = (Data*)user_data;
            data->material = Common::CreateStringId(name);
        };
        callbacks.mtllib_cb = [](void *user_data, const material_t *materials, int num_materials) {};

        callbacks.group_cb = [](void *user_data, const char **names, int num_names) {};
        callbacks.object_cb = [](void *user_data, const char *name) {

            Data *data = (Data*)user_data;
            
            if (data->objects.back().faceCount > 0) {
                data->objects.emplace_back();
            }

            auto &object = data->objects.back();
            object.firstFace = data->faces.size();
            object.name = Common::CreateStringId(name);
        };

        Data data;
        data.objects.emplace_back();
        data.filename = filename.c_str();
        std::string error;
        if (!tinyobj::LoadObjWithCallback(stream, callbacks, &data, nullptr, &error)) {
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

        for (auto iter=data.objects.begin(), end=data.objects.end(); iter != end; ++iter) {
            Object object;

            object.name = iter->name;
            indexLookup.clear();

            SubObject subObject;

            for (size_t i=0, c=iter->faceCount; i < c; ++i) {
                Face face = data.faces[i + iter->firstFace];

                if (subObject.count > 0 && subObject.material != face.material) {
                    object.subobjects.push_back(subObject);
                    subObject.material = face.material;
                    subObject.first = (int)object.indexes.size();
                    subObject.count = 0;
                }
                else {
                    subObject.count++;
                }

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

                        object.vertexes.push_back(vertex);
                        idx = (int)object.vertexes.size() - 1;

                        indexLookup[index] = idx;
                    }

                    object.indexes.push_back(idx);
                }
            }
            object.subobjects.push_back(subObject);
            result.objects.push_back(object);
        }

        return std::move(result);
    }

}