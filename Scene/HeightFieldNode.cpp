// Landscape node.
// -------------------------------------------------------------------
// Copyright (C) 2007 OpenEngine.dk (See AUTHORS) 
// 
// This program is free software; It is covered by the GNU General 
// Public License version 2 or any later version. 
// See the GNU General Public License for more details (see LICENSE). 
//--------------------------------------------------------------------

#include <Scene/HeightFieldNode.h>
#include <Scene/HeightFieldPatchNode.h>
#include <Math/Math.h>
#include <Meta/OpenGL.h>
#include <Utils/TerrainUtils.h>
#include <Renderers/OpenGL/TextureLoader.h>
#include <Display/IViewingVolume.h>

#include <Logging/Logger.h>

#include <algorithm>

#define USE_PATCHES true

using namespace OpenEngine::Renderers::OpenGL;
using namespace OpenEngine::Display;

namespace OpenEngine {
    namespace Scene {
        
        HeightFieldNode::HeightFieldNode(UCharTexture2DPtr tex)
            : tex(tex) {
            tex->Load();
            heightScale = 1;
            widthScale = 1;
            waterlevel = 10;
            offset = Vector<3, float>(0, 0, 0);
            
            texDetail = 1;
            baseDistance = 1;
            invIncDistance = 1.0f / 100.0f;

            texCoords = NULL;
        }

        HeightFieldNode::~HeightFieldNode(){
            delete [] vertices;
            delete [] normals;
            delete [] geomorphValues;
            delete [] texCoords;
            delete [] normalMapCoords;
            delete [] indices;
            delete [] deltaValues;
            delete [] indices;

            delete [] patchNodes;
        }
        
        void HeightFieldNode::Load() {
            InitArrays();
            if (USE_PATCHES)
                SetupPatches();
            else
                ComputeIndices();
        }

        void HeightFieldNode::CalcLOD(IViewingVolume* view){
            if (USE_PATCHES)
                for (int i = 0; i < numberOfPatches; ++i)
                    patchNodes[i]->CalcLOD(view);
        }

        void HeightFieldNode::Render(IViewingVolume* view){
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indiceId);
            if (USE_PATCHES){
                // Draw patches front to back.
                Vector<3, float> dir = view->GetDirection().RotateVector(Vector<3, float>(0,0,1));

                int xStart, xEnd, xStep, zStart, zEnd, zStep;
                if (dir[0] < 0){
                    // If we're looking along the x-axis.
                    xStart = 0;
                    xEnd = patchGridWidth;
                    xStep = 1;
                }else{
                    // else iterate form the other side.
                    xStart = patchGridWidth-1;
                    xEnd = -1;
                    xStep = -1;
                }
                if (dir[2] < 0){
                    // If we're looking along the z-axis.
                    zStart = 0;
                    zEnd = patchGridDepth;
                    zStep = 1;
                }else{
                    // else iterate form the other side.
                    zStart = patchGridDepth-1;
                    zEnd = -1;
                    zStep = -1;
                }
                
                for (int x = xStart; x != xEnd ; x += xStep){
                    for (int z = zStart; z != zEnd; z += zStep){
                        patchNodes[z + x * patchGridDepth]->Render();
                    }
                }
            }else{
                glDrawElements(GL_TRIANGLE_STRIP, numberOfIndices, GL_UNSIGNED_INT, 0);
            }
        }

        void HeightFieldNode::RenderBoundingGeometry(){
            if (USE_PATCHES)
                for (int i = 0; i < numberOfPatches; ++i)
                    patchNodes[i]->RenderBoundingGeometry();
        }

        void HeightFieldNode::VisitSubNodes(ISceneNodeVisitor& visitor){
            list<ISceneNode*>::iterator itr;
            for (itr = subNodes.begin(); itr != subNodes.end(); ++itr){
                (*itr)->Accept(visitor);
            }
        }

        void HeightFieldNode::Handle(RenderingEventArg arg){
            Load();

            // Create vbos

            // Vertice buffer object
            glGenBuffers(1, &verticeBufferId);
            glBindBuffer(GL_ARRAY_BUFFER, verticeBufferId);
            glBufferData(GL_ARRAY_BUFFER, 
                         sizeof(GLfloat) * numberOfVertices * DIMENSIONS,
                         vertices, GL_STATIC_DRAW);
            
            // Tex Coord buffer object
            glGenBuffers(1, &texCoordBufferId);
            glBindBuffer(GL_ARRAY_BUFFER, texCoordBufferId);
            glBufferData(GL_ARRAY_BUFFER, 
                         sizeof(GLfloat) * numberOfVertices * TEXCOORDS,
                         texCoords, GL_STATIC_DRAW);

            glBindBuffer(GL_ARRAY_BUFFER, 0);

            // Create indice buffer
            glGenBuffers(1, &indiceId);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indiceId);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                         sizeof(GLuint) * numberOfIndices,
                         indices, GL_STATIC_DRAW);
            
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

            if (landscapeShader != NULL) {
                // Init shader used buffer objects

                // normal map pbo
                glGenBuffers(1, &normalsBufferId);
                glBindBuffer(GL_PIXEL_UNPACK_BUFFER, normalsBufferId);
                glBufferData(GL_PIXEL_UNPACK_BUFFER, 
                             sizeof(GLfloat) * numberOfVertices * 3,
                             normals, GL_STATIC_DRAW);

                // Create the image to hold the normal map
                unsigned int normalTexId;
                glGenTextures(1, &normalTexId);
                glBindTexture(GL_TEXTURE_2D, normalTexId);
                glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S, GL_REPEAT);
                glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T, GL_REPEAT);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, depth, width, 0, GL_RGB, GL_FLOAT, NULL);

                glBindTexture(GL_TEXTURE_2D, 0);
                glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

                normalmap = FloatTexture2DPtr(new Texture2D<float>(width, depth, 3));
                normalmap->SetID(normalTexId);

                // Geomorph values buffer object
                glGenBuffers(1, &geomorphBufferId);
                glBindBuffer(GL_ARRAY_BUFFER, geomorphBufferId);
                glBufferData(GL_ARRAY_BUFFER, 
                             sizeof(GLfloat) * numberOfVertices * 3,
                             geomorphValues, GL_STATIC_DRAW);                

                // normal map Coord buffer object
                glGenBuffers(1, &normalMapCoordBufferId);
                glBindBuffer(GL_ARRAY_BUFFER, normalMapCoordBufferId);
                glBufferData(GL_ARRAY_BUFFER, 
                             sizeof(GLfloat) * numberOfVertices * TEXCOORDS,
                             normalMapCoords, GL_STATIC_DRAW);

                landscapeShader->Load();
                TextureList texs = landscapeShader->GetTextures();
                for (unsigned int i = 0; i < texs.size(); ++i)
                    TextureLoader::LoadTextureResource(texs[i]);

                TextureLoader::LoadTextureResource(normalmap, true, false);

                landscapeShader->ApplyShader();

                /*
                 * Should be moved to main. Terrain shouldn't make any
                 * assumptions on the texturing.
                 *
                 * Shader needs to store uniforms for this to work and
                 * bind them to the gpu when applied. (Incoming)
                 *
                landscapeShader->SetUniform("snowStartHeight", (float)50);
                landscapeShader->SetUniform("snowBlend", (float)20);
                landscapeShader->SetUniform("grassStartHeight", (float)5);
                landscapeShader->SetUniform("grassBlend", (float)5);
                landscapeShader->SetUniform("sandStartHeight", (float)-10);
                landscapeShader->SetUniform("sandBlend", (float)10);
                */

                landscapeShader->SetTexture("normalMap", normalmap);

                landscapeShader->ReleaseShader();
            }
            
            SetLODSwitchDistance(baseDistance, 1 / invIncDistance);

            // Cleanup in ram
            delete [] geomorphValues;
            geomorphValues = NULL;
            delete [] texCoords;
            texCoords = NULL;
            delete [] normalMapCoords;
            normalMapCoords = NULL;
            delete [] indices;
            indices = NULL;
        }
        
        // **** Get/Set methods ****

        float HeightFieldNode::GetHeight(Vector<3, float> point) const{
            return GetHeight(point[0], point[2]);
        }

        float HeightFieldNode::GetHeight(float x, float z) const{
            /**
             * http://en.wikipedia.org/wiki/Bilinear_interpolation
             */
            
            // x and z normalized with respect to the scaling.
            // @TODO If a translation is ever used it should go here aswell.
            x /= widthScale;
            z /= widthScale;
            
            // The indices into the array
            int X = floor(x);
            int Z = floor(z);
            
            float dX = x - X;
            float dZ = z - Z;

            // Bilinear interpolation of the heights.
            float height = GetVertice(X, Z)[1] * (1-dX) * (1-dZ) +
                           GetVertice(X+1, Z)[1] * dX * (1-dZ) +
                           GetVertice(X, Z+1)[1] * (1-dX) * dZ +
                           GetVertice(X+1, Z+1)[1] * dX * dZ;
            
            return height;
        }

        float HeightFieldNode::GetNormal(Vector<3, float> point) const{
            return GetNormal(point[0], point[2]);
        }

        float HeightFieldNode::GetNormal(float x, float z) const{
            /**
             * http://en.wikipedia.org/wiki/Bilinear_interpolation
             */
            
            // x and z normalized with respect to the scaling.
            // @TODO If a translation is ever used it should go here aswell.
            x /= widthScale;
            z /= widthScale;
            
            // The indices into the array
            int X = floor(x);
            int Z = floor(z);
            
            float dX = x - X;
            float dZ = z - Z;

            // Bilinear interpolation of the heights.
            float height = GetNormals(X, Z)[1] * (1-dX) * (1-dZ) +
                           GetNormals(X+1, Z)[1] * dX * (1-dZ) +
                           GetNormals(X, Z+1)[1] * (1-dX) * dZ +
                           GetNormals(X+1, Z+1)[1] * dX * dZ;
            
            return height;
        }

        int HeightFieldNode::GetIndice(int x, int z){
            return CoordToIndex(x, z);
        }

        float* HeightFieldNode::GetVertex(int x, int z){
            if (x < 0)
                x = 0;
            else if (x >= width)
                x = width - 1;

            if (z < 0)
                z = 0;
            else if (z >= depth)
                z = depth - 1;
            return GetVertice(x, z);
        }

        void HeightFieldNode::SetVertex(int x, int z, float value){
            glBindBuffer(GL_ARRAY_BUFFER, verticeBufferId);
            float* vbo = (float*) glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);

            // Update height for the moved vertice affected.
            int index = CoordToIndex(x, z);
            vbo[index * DIMENSIONS + 1] = GetVertice(index)[1] = value;
            vbo[index * DIMENSIONS + 3] = GetVertice(index)[3] = CalcGeomorphHeight(x, z);

            // Update morphing height for all surrounding affected
            // vertices.
            for (int delta = GetVerticeDelta(index) / 2; delta >= 1; delta /= 2){
                if (0 <= x-delta){
                    index = CoordToIndex(x-delta, z);
                    vbo[index * DIMENSIONS + 3] = GetVertice(index)[3] = CalcGeomorphHeight(x-delta, z);
                }
                
                if (x+delta < width){
                    index = CoordToIndex(x+delta, z);
                    vbo[index * DIMENSIONS + 3] = GetVertice(index)[3] = CalcGeomorphHeight(x+delta, z);
                }
                
                if (0 <= z-delta){
                    index = CoordToIndex(x, z-delta);
                    vbo[index * DIMENSIONS + 3] = GetVertice(index)[3] = CalcGeomorphHeight(x, z-delta);
                }

                if (z+delta < depth){
                    index = CoordToIndex(x, z+delta);
                    vbo[index * DIMENSIONS + 3] = GetVertice(index)[3] = CalcGeomorphHeight(x, z+delta);
                }

                if (0 <= x-delta && 0 <= z-delta){
                    index = CoordToIndex(x-delta, z-delta);
                    vbo[index * DIMENSIONS + 3] = GetVertice(index)[3] = CalcGeomorphHeight(x-delta, z-delta);
                }

                if (x+delta < width && z+delta < depth){
                    index = CoordToIndex(x+delta, z+delta);
                    vbo[index * DIMENSIONS + 3] = GetVertice(index)[3] = CalcGeomorphHeight(x+delta, z+delta);
                }
            }

            glUnmapBuffer(GL_ARRAY_BUFFER);
            glBindBuffer(GL_ARRAY_BUFFER, 0);

            // Update shadows
            int shadowLeft = x < 2 ? 0 : x - 1;
            int shadowRight = x + 3 > width ? width : x + 2;
            int shadowBelow = z < 2 ? 0 : z - 1;
            int shadowAbove = z + 3 > depth ? depth : z + 2;

            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, normalsBufferId);
            float* pbo = (float*) glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
            int pboIndex = 0;
            for (int xi = shadowLeft; xi < shadowRight; ++xi)
                for (int zi = shadowBelow; zi < shadowAbove; ++zi){
                    Vector<3, float> normal = GetNormal(xi, zi);
                    normal.ToArray(GetNormals(xi, zi));
                    normal.ToArray(pbo + pboIndex);
                    pboIndex += 3;
                }

            glBindTexture(GL_TEXTURE_2D, normalmap->GetID());

            glTexSubImage2D(GL_TEXTURE_2D, 0, shadowLeft, shadowBelow, 
                            shadowRight - shadowLeft, shadowAbove - shadowBelow, 
                            GL_RGB, GL_FLOAT, 0);

            glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
            glBindTexture(GL_TEXTURE_2D, 0);
            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);            

            // Update bounding box
            HeightFieldPatchNode* mainNode = GetPatch(x, z);
            mainNode->UpdateBoundingGeometry(value);
            HeightFieldPatchNode* upperNode = GetPatch(x+1, z);
            if (upperNode != mainNode) upperNode->UpdateBoundingGeometry(value);
            HeightFieldPatchNode* rightNode = GetPatch(x, z+1);
            if (rightNode != mainNode) rightNode->UpdateBoundingGeometry(value);
            HeightFieldPatchNode* upperRightNode = GetPatch(x+1, z+1);
            if (upperRightNode != mainNode) upperRightNode->UpdateBoundingGeometry(value);

        }

        void HeightFieldNode::SetVertices(int x, int z, int w, int d, float* values){

            //          Above
            //        z
            //        A
            //        |
            // Left   |           Right
            //        |
            //        |
            //       -+-------> x 
            // 
            //          Below

            // if the area is outside the heightmap
            if (x >= width || z >= depth || x + w <= 0 || z + d <= 0) return;

            // Update the height for the moved vertices
            int xStart = x < 0 ? 0 : x;
            int zStart = z < 0 ? 0 : z;
            int xEnd = (x + w >= width) ? width : x + w;
            int zEnd = (z + d >= depth) ? depth : z + d;
            
            glBindBuffer(GL_ARRAY_BUFFER, verticeBufferId);
            float* vbo = (float*) glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);
            for (int xi = xStart; xi < xEnd; ++xi)
                for (int zi = zStart; zi < zEnd; ++zi){
                    int index = CoordToIndex(xi, zi);
                    vbo[index * DIMENSIONS + 1] = GetVertice(index)[1] = values[(zi - z) + (xi - x) * d];
                }

            // Update the morphing height for all affected vertices
            int morphLeft = xStart - HeightFieldPatchNode::MAX_DELTA < 0 ? 0 : xStart - HeightFieldPatchNode::MAX_DELTA;
            int morphRight = xEnd + HeightFieldPatchNode::MAX_DELTA > width ? width : xEnd + HeightFieldPatchNode::MAX_DELTA;
            int morphBelow = zStart - HeightFieldPatchNode::MAX_DELTA < 0 ? 0 : zStart - HeightFieldPatchNode::MAX_DELTA;;
            int morphAbove = zEnd + HeightFieldPatchNode::MAX_DELTA > depth ? depth : zEnd + HeightFieldPatchNode::MAX_DELTA;

            for (int xi = morphLeft; xi < morphRight; ++xi)
                for (int zi = morphBelow; zi < morphAbove; ++zi){
                    int index = CoordToIndex(xi, zi);
                    vbo[index * DIMENSIONS + 3] = GetVertice(index)[3] = CalcGeomorphHeight(xi, zi);
                }

            glUnmapBuffer(GL_ARRAY_BUFFER);
            glBindBuffer(GL_ARRAY_BUFFER, 0);

            // Update the shadows
            int shadowLeft = xStart < 2 ? 0 : xStart - 1;
            int shadowRight = xEnd + 3 > width ? width : xEnd + 2;
            int shadowBelow = zStart < 2 ? 0 : zStart - 1;
            int shadowAbove = zEnd + 3 > depth ? depth : zEnd + 2;

            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, normalsBufferId);
            float* pbo = (float*) glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
            int pboIndex = 0;
            for (int xi = shadowLeft; xi < shadowRight; ++xi)
                for (int zi = shadowBelow; zi < shadowAbove; ++zi){
                    Vector<3, float> normal = GetNormal(xi, zi);
                    normal.ToArray(GetNormals(xi, zi));
                    normal.ToArray(pbo + pboIndex);
                    pboIndex += 3;
                }

            glBindTexture(GL_TEXTURE_2D, normalmap->GetID());

            glTexSubImage2D(GL_TEXTURE_2D, 0, shadowLeft, shadowBelow, 
                            shadowRight - shadowLeft, shadowAbove - shadowBelow, 
                            GL_RGB, GL_FLOAT, 0);

            glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
            glBindTexture(GL_TEXTURE_2D, 0);
            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

            // Update the bounding geometry
            int patchSize = HeightFieldPatchNode::PATCH_EDGE_SQUARES;
            int xBoundingStart = (xStart / patchSize) * patchSize;
            int zBoundingStart = (zStart / patchSize) * patchSize;
            for (int xi = xBoundingStart; xi < xEnd; xi += patchSize)
                for (int zi = zBoundingStart; zi < zEnd; zi += patchSize){
                    GetPatch(xi, zi)->UpdateBoundingGeometry();
                }
        }

        Vector<3, float> HeightFieldNode::GetNormal(int x, int z){
            
            Vector<3, float> normal = Vector<3, float>(0.0f);
            float vHeight = GetVertice(x, z)[1];

            // Right vertex
            if (x + 1 < width){
                float wHeight = GetVertice(x + 1, z)[1];
                normal[0] += vHeight - wHeight;
                normal[1] += widthScale;
            }
            
            // Left vertex
            if (0 < x){
                float wHeight = GetVertice(x - 1, z)[1];
                normal[0] += wHeight - vHeight;
                normal[1] += widthScale;
            }

            // upper vertex
            if (z + 1 < depth){
                float wHeight = GetVertice(x, z + 1)[1];
                normal[2] += vHeight - wHeight;
                normal[1] += widthScale;
            }
            
            // Lower vertex
            if (0 < z){
                float wHeight = GetVertice(x, z - 1)[1];
                normal[2] += wHeight - vHeight;
                normal[1] += widthScale;
            }

            normal.Normalize();

            return normal;
            
        }
        
        /**
         * Set the distance at which the LOD should switch.
         *
         * @ base The base distance to the camera where the LOD is the highest.
         * @ dec The distance between each decrement in LOD.
         */
        void HeightFieldNode::SetLODSwitchDistance(float base, float dec){
            baseDistance = base;
            
            float edgeLength = HeightFieldPatchNode::PATCH_EDGE_SQUARES * widthScale;
            if (dec * dec < edgeLength * edgeLength * 2){
                invIncDistance = 1.0f / sqrt(edgeLength * edgeLength * 2);
                logger.error << "Incremental LOD distance is too low, setting it to lowest value: " << 1.0f / invIncDistance << logger.end;
            }else
                invIncDistance = 1.0f / dec;

            // Update uniforms
            if (landscapeShader != NULL) {
                landscapeShader->ApplyShader();

                landscapeShader->SetUniform("baseDistance", baseDistance);
                landscapeShader->SetUniform("invIncDistance", invIncDistance);

                landscapeShader->ReleaseShader();
            }
        }

        void HeightFieldNode::SetTextureDetail(const float detail){
            texDetail = detail;
            if (texCoords)
                SetupTerrainTexture();
        }

        // **** inline functions ****

        void HeightFieldNode::InitArrays(){
            int texWidth = tex->GetHeight();
            int texDepth = tex->GetWidth();

            // if texwidth/depth isn't expressible as n * patchwidth + 1 fix it.
            int patchWidth = HeightFieldPatchNode::PATCH_EDGE_SQUARES;
            int widthRest = (texWidth - 1) % patchWidth;
            width = widthRest ? texWidth + patchWidth - widthRest : texWidth;

            int depthRest = (texDepth - 1) % patchWidth;
            depth = depthRest ? texDepth + patchWidth - depthRest : texDepth;

            numberOfVertices = width * depth;

            vertices = new float[numberOfVertices * DIMENSIONS];
            normals = new float[numberOfVertices * 3];
            texCoords = new float[numberOfVertices * TEXCOORDS];
            normalMapCoords = new float[numberOfVertices * TEXCOORDS];
            geomorphValues = new float[numberOfVertices * 3];
            deltaValues = new char[numberOfVertices];

            unsigned char* data = tex->GetData();

            // Fill the vertex array
            int d = tex->GetChannels() - 1;
            for (int x = 0; x < width; ++x){
                for (int z = 0; z < depth; ++z){
                    float* vertice = GetVertice(x, z);
                     
                    vertice[0] = widthScale * x + offset[0];
                    vertice[2] = widthScale * z + offset[2];
                    if (DIMENSIONS > 3)
                        vertice[3] = 1;
       
                    if (x < texWidth && z < texDepth){
                        // inside the heightmap
                        float height = (float)data[d];
                        d += tex->GetChannels();
                        vertice[1] = height * heightScale - waterlevel - heightScale / 2 + offset[1];
                    }else{
                        // outside the heightmap, set height to waterlevel
                        vertice[1] = -waterlevel - heightScale / 2 + offset[1];
                    }
                }
            }
            
            SetupNormalMap();
            SetupTerrainTexture();

            if (landscapeShader != NULL)
                CalcVerticeLOD();
                for (int x = 0; x < width; ++x)
                    for (int z = 0; z < depth; ++z){
                        // Store the morphing value in the w-coord to
                        // use in the shader.
                        float* vertice = GetVertice(x, z);
                        vertice[3] = CalcGeomorphHeight(x, z);
                    }
        }

        void HeightFieldNode::SetupNormalMap(){
            for (int x = 0; x < width; ++x)
                for (int z = 0; z < depth; ++z){
                    float* coord = GetNormalMapCoord(x, z);
                    coord[1] = (x + 0.5f) / (float) width;
                    coord[0] = (z + 0.5f) / (float) depth;
                    Vector<3, float> normal = GetNormal(x, z);
                    normal.ToArray(GetNormals(x, z));
                }
        }

        void HeightFieldNode::SetupTerrainTexture(){
            for (int x = 0; x < width; ++x){
                for (int z = 0; z < depth; ++z){
                    CalcTexCoords(x, z);
                }
            }
        }

        void HeightFieldNode::CalcTexCoords(int x, int z){
            float* texCoord = GetTexCoord(x, z);
            texCoord[1] = x * texDetail;
            texCoord[0] = z * texDetail;
        }

        void HeightFieldNode::CalcVerticeLOD(){
            for (int LOD = 1; LOD <= HeightFieldPatchNode::MAX_LODS; ++LOD){
                int delta = pow(2, LOD-1);
                for (int x = 0; x < width; x += delta){
                    for (int z = 0; z < depth; z += delta){
                        GetVerticeLOD(x, z) = LOD;
                        GetVerticeDelta(x, z) = pow(2, LOD-1);
                    }
                }
            }
        }

        float HeightFieldNode::CalcGeomorphHeight(int x, int z){
            short delta = GetVerticeDelta(x, z);

            int dx, dz;
            if (delta < HeightFieldPatchNode::MAX_DELTA){
                dx = x % (delta * 2);
                dz = z % (delta * 2);
            }else{
                dx = 0;
                dz = 0;
            }

            float* vertice = GetVertice(x, z);
            float* verticeNeighbour1 = GetVertice(x + dx, z + dz);
            float* verticeNeighbour2 = GetVertice(x - dx, z - dz);

            return (verticeNeighbour1[1] + verticeNeighbour2[1]) / 2 - vertice[1];
        }

        void HeightFieldNode::ComputeIndices(){
            int LOD = 4;
            int xs = (width-1) / LOD + 1;
            int zs = (depth-1) / LOD + 1;
            numberOfIndices = 2 * ((xs - 1) * zs + xs - 2);
            indices = new unsigned int[numberOfIndices];

            unsigned int i = 0;
            for (int x = 0; x < width - 1; x += LOD){
                for (int z = depth - 1; z >= 0; z -= LOD){
                    indices[i++] = CoordToIndex(x, z);
                    indices[i++] = CoordToIndex(x+LOD, z);
                }
                if (x < depth - 1 - LOD){
                    indices[i++] = CoordToIndex(x+LOD, 0);
                    indices[i++] = CoordToIndex(x+LOD, width - 1);
                }
            }

            if (i < numberOfIndices){
                logger.info << "Allocated to much memory for the indices, lets get lower" << logger.end;
                numberOfIndices = i;
            }else if (i > numberOfIndices){
                logger.info << "You're about to crash monsiour. Good luck. Allocated " << numberOfIndices << " but used " << i << logger.end;
                numberOfIndices = i;
            }
        }

        void HeightFieldNode::SetupPatches(){
            // Create the patches
            int squares = HeightFieldPatchNode::PATCH_EDGE_SQUARES;
            patchGridWidth = (width-1) / squares;
            patchGridDepth = (depth-1) / squares;
            numberOfPatches = patchGridWidth * patchGridDepth;
            patchNodes = new HeightFieldPatchNode*[numberOfPatches];
            int entry = 0;
            for (int x = 0; x < width - squares; x +=squares ){
                for (int z = 0; z < depth - squares; z += squares){
                    patchNodes[entry++] = new HeightFieldPatchNode(x, z, this);
                }
            }

            // Link the patches
            for (int x = 0; x < patchGridWidth; ++x){
                for (int z = 0; z < patchGridDepth; ++z){
                    int entry = z + x * patchGridDepth;
                    if (x + 1 < patchGridWidth)
                        patchNodes[entry]->SetUpperNeighbor(patchNodes[entry + patchGridDepth]);
                    if (z + 1 < patchGridDepth) 
                        patchNodes[entry]->SetRightNeighbor(patchNodes[entry + 1]);
                }
            }

            // Setup indice buffer
            numberOfIndices = 0;
            for (int p = 0; p < numberOfPatches; ++p){
                for (int l = 0; l < HeightFieldPatchNode::MAX_LODS; ++l){
                    for (int rl = 0; rl < 3; ++rl){
                        for (int ul = 0; ul < 3; ++ul){
                            LODstruct& lod = patchNodes[p]->GetLodStruct(l,rl,ul);
                            lod.indiceBufferOffset = (void*)(numberOfIndices * sizeof(GLuint));
                            numberOfIndices += lod.numberOfIndices;
                        }
                    }
                }
            }

            indices = new unsigned int[numberOfIndices];

            unsigned int i = 0;
            for (int p = 0; p < numberOfPatches; ++p){
                for (int l = 0; l < HeightFieldPatchNode::MAX_LODS; ++l){
                    for (int rl = 0; rl < 3; ++rl){
                        for (int ul = 0; ul < 3; ++ul){
                            LODstruct& lod = patchNodes[p]->GetLodStruct(l,rl,ul);
                            memcpy(indices + i, lod.indices, sizeof(unsigned int) * lod.numberOfIndices);
                            i += lod.numberOfIndices;
                        }
                    }
                }
            }
                        
            // Setup shader uniforms used in geomorphing
            for (int x = 0; x < width - 1; ++x){
                for (int z = 0; z < depth - 1; ++z){
                    HeightFieldPatchNode* patch = GetPatch(x, z);
                    float* geomorph = GetGeomorphValues(x, z);
                    geomorph[0] = patch->GetCenter()[0];
                    geomorph[1] = patch->GetCenter()[2];
                }
            }
        }
        
        int HeightFieldNode::CoordToIndex(const int x, const int z) const{
            return z + x * depth;
        }
        
        float* HeightFieldNode::GetVertice(const int x, const int z) const{
            int index = CoordToIndex(x, z);
            return GetVertice(index);
        }

        float* HeightFieldNode::GetVertice(const int index) const{
            return vertices + index * DIMENSIONS;
        }
        
        float* HeightFieldNode::GetNormals(const int x, const int z) const{
            int index = CoordToIndex(x, z);
            return GetNormals(index);
        }

        float* HeightFieldNode::GetNormals(const int index) const{
            return normals + index * 3;
        }

        float* HeightFieldNode::GetTexCoord(const int x, const int z) const{
            int index = CoordToIndex(x, z);
            return texCoords + index * TEXCOORDS;
        }

        float* HeightFieldNode::GetNormalMapCoord(const int x, const int z) const{
            int index = CoordToIndex(x, z);
            return normalMapCoords + index * TEXCOORDS;
        }

        float* HeightFieldNode::GetGeomorphValues(const int x, const int z) const{
            int index = CoordToIndex(x, z);
            return geomorphValues + index * 3;
        }

        float& HeightFieldNode::GetVerticeLOD(const int x, const int z) const{
            int index = CoordToIndex(x, z);
            return GetVerticeLOD(index);
        }

        float& HeightFieldNode::GetVerticeLOD(const int index) const{
            return (geomorphValues + index * 3)[2];
        }

        char& HeightFieldNode::GetVerticeDelta(const int x, const int z) const{
            int index = CoordToIndex(x, z);
            return GetVerticeDelta(index);
        }

        char& HeightFieldNode::GetVerticeDelta(const int index) const{
            return deltaValues[index];
        }

        int HeightFieldNode::GetPatchIndex(const int x, const int z) const{
            int patchX = (x-1) / HeightFieldPatchNode::PATCH_EDGE_SQUARES;
            int patchZ = (z-1) / HeightFieldPatchNode::PATCH_EDGE_SQUARES;
            return patchZ + patchX * patchGridDepth;
        }

        HeightFieldPatchNode* HeightFieldNode::GetPatch(const int x, const int z) const{
            int index = GetPatchIndex(x, z);
            return patchNodes[index];
        }

    }
}
