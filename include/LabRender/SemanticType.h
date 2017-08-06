//
//  SemanticType.h
//  LabApp
//
//  Created by Nick Porcino on 2014 02/28.
//  Copyright (c) 2014 Nick Porcino. All rights reserved.
//

#pragma once

#include <LabRender/LabRender.h>
#include <set>
#include <string>
#include <vector>

namespace lab {

	int semanticTypeToOpenGLElementType(SemanticType t);
	int semanticTypeElementCount(SemanticType t);
	int semanticTypeStride(SemanticType t);
	const char* semanticTypeName(SemanticType t);
	SemanticType semanticTypeNamed(const char*);
	std::string semanticTypeToString(SemanticType st);


	class Uniform {
	public:
		Uniform(std::string name,
			SemanticType type,
			AutomaticUniform automatic,
			std::string texture)
			: name(name), type(type), automatic(automatic), texture(texture) {}

		Uniform(const Uniform & rhs)
			: name(rhs.name), type(rhs.type), automatic(rhs.automatic), texture(rhs.texture) {}

		Uniform & operator=(const Uniform & rhs) {
			name = rhs.name; type = rhs.type; automatic = rhs.automatic; texture = rhs.texture;
			return *this;
		}

		std::string name;
		std::string texture;
		SemanticType type = SemanticType::unknown_st;
		AutomaticUniform automatic = AutomaticUniform::none;
	};


	class Semantic {
	public:
		Semantic()
			: type(SemanticType::unknown_st), automatic(AutomaticUniform::none), location(0) {}

		Semantic(SemanticType type, char const*const name, int location)
			: type(type), name(name), automatic(AutomaticUniform::none), location(location) { }

		Semantic(SemanticType type, const std::string& name, int location)
			: type(type), name(name), automatic(AutomaticUniform::none), location(location) { }

		Semantic(SemanticType type, const std::string& name, AutomaticUniform a, int location)
			: type(type), name(name), automatic(a), location(location) { }

		Semantic(const Semantic& rhs)
			: type(rhs.type), name(rhs.name), automatic(rhs.automatic), location(rhs.location) {}

		static std::set<Semantic*> makeSemantics(const std::vector<Uniform> & semantics) {
			std::set<Semantic*> result;
			int l = 0;
			for (auto i : semantics) {
				result.insert(new Semantic(i.type, i.name, i.automatic, l++));
			}
			return result;
		}

		static std::set<Semantic*> makeSemantics(const std::vector<std::pair<std::string, SemanticType>>& semantics) {
			std::set<Semantic*> result;
			int l = 0;
			for (auto i : semantics) {
				result.insert(new Semantic(i.second, i.first, l++));
			}
			return result;
		}


		std::string uniformString() {
			std::string result = "uniform " + semanticTypeToString(type) + " ";
			result += name + ";";
			return result;
		}

		std::string attributeString() {
			std::string result = "layout(location = ";
			char buff[32];
			snprintf(buff, 32, "%d", location);
			result += buff;
			result += ") in " + semanticTypeToString(type) + " ";
			result += name + ";";
			return result;
		}

		std::string attachmentsString() {
			std::string result = "layout(location = ";
			char buff[32];
			snprintf(buff, 32, "%d", location);
			result += buff;
			result += ", index = 0) out " + semanticTypeToString(type) + " ";
			result += name + ";";
			return result;
		}

		std::string varyingsOutString() {
			std::string result = "out " + semanticTypeToString(type) + " ";
			result += name + ";";
			return result;
		}

		std::string varyingsInString() {
			std::string result = "in " + semanticTypeToString(type) + " ";
			result += name + ";";
			return result;
		}

		std::string outputString() {
			std::string result = "layout(location = ";
			char buff[32];
			snprintf(buff, 32, "%d", location);
			result += buff;
			result += ") out " + semanticTypeToString(type) + " ";
			result += name + ";";
			return result;
		}

		SemanticType type = SemanticType::unknown_st;
		std::string name;
		AutomaticUniform automatic = AutomaticUniform::none;
		int location = 0;
	};

} //lab
