//
//  Utils.h
//  LabApp
//
//  Created by Nick Porcino on 2014 04/5.
//  Copyright (c) 2014 Nick Porcino. All rights reserved.
//

#pragma once

#include <LabRender/LabRender.h>
#include <string>

namespace lab {

    // expected: $(ASSET_ROOT) resolvedCwd/assets
    // add as many others as necessary
    //
    LR_API void addPathVariable(const std::string& var, const std::string& replacement);
    
    LR_API std::string loadFile(char const*const path, bool errorIfNotFound = true);
    LR_API std::string expandPath(char const*const path);
}

