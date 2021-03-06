// Sun node.
// -------------------------------------------------------------------
// Copyright (C) 2007 OpenEngine.dk (See AUTHORS) 
// 
// This program is free software; It is covered by the GNU General 
// Public License version 2 or any later version. 
// See the GNU General Public License for more details (see LICENSE). 
//--------------------------------------------------------------------

#ifndef _SUN_NODE_H_
#define _SUN_NODE_H_

#include <Scene/ISceneNode.h>
#include <Core/IListener.h>
#include <Renderers/IRenderer.h>
#include <Core/EngineEvents.h>

using namespace OpenEngine;
using namespace OpenEngine::Core;
using namespace OpenEngine::Renderers;

namespace OpenEngine {
    namespace Scene {
        
        class SunNode : public ISceneNode, public IListener<Core::ProcessEventArg> {
            OE_SCENE_NODE(sunNode, ISceneNode)
            
        private:
            Vector<3, float> coords;
            Vector<3, float> origo;
            Vector<3, float> direction;

            Vector<4, float> ambient;
            Vector<4, float> diffuse;
            Vector<4, float> baseDiffuse;
            Vector<4, float> specular;
            Vector<4, float> baseSpecular;

            float time;
            float dayLength; // length of day in miliseconds
            
            bool geometry;

        public:
            SunNode(){coords[0] = 0; coords[1] = 0; coords[2] = 0; }
            SunNode(Vector<3, float> coords);
            SunNode(Vector<3, float> coords, Vector<3, float> origo);

            Vector<3, float> GetPos() { return coords; }

            void SetTime(unsigned int time) { this->time = time; }
            void SetDayLength(float dayLength) { this->dayLength = dayLength * 1000000; }
            float GetDayLength() { return dayLength / 1000000; }
            void SetTimeOfDay(float time);
            float GetTimeOfDay();
            float GetTimeofDayRatio();

            void SetAmbient(Vector<4, float> a) { ambient = a; }
            void SetDiffuse(Vector<4, float> d) { baseDiffuse = d; }
            void SetSpecular(Vector<4, float> s) { baseSpecular = s; }
            Vector<4, float> GetAmbient() { return ambient; }
            Vector<4, float> GetDiffuse() { return diffuse; }
            Vector<4, float> GetSpecular() { return specular; }

            void Move(unsigned int dt);

            void VisitSubNodes(ISceneNodeVisitor& visitor);

            void Handle(Core::ProcessEventArg arg);

            void SetRenderGeometry(bool g) { geometry = g; }
            bool renderGeometry() { return geometry;}

        private:
            void Init(Vector<3, float> coords, Vector<3, float> origo);
        };
        
    }
}

#endif //_SUN_NODE_H_
