//
//  DrawList.h
//  LabRender
//
//  Created by Nick Porcino on 5/29/15.
//  Copyright (c) 2015 Planet IX. All rights reserved.
//

#pragma once

#include "LabRender/Light.h"
#include "LabRender/MathTypes.h"
#include "LabRender/ModelBase.h"
#include <memory>
#include <vector>

namespace lab {

    class DrawList 
	{
    public:
        DrawList()
        {
            jacobian = m44f_identity;
            view = m44f_identity;
            proj = m44f_identity;
        }

        std::vector<std::shared_ptr<ModelBase>> deferredMeshes;
        std::vector<std::shared_ptr<Illuminant>> lights;

        m44f jacobian;
        m44f view;
        m44f proj;
    };

}
