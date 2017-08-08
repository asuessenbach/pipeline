// Copyright (c) 2015-2016, NVIDIA CORPORATION. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
//  * Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
//  * Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//  * Neither the name of NVIDIA CORPORATION nor the names of its
//    contributors may be used to endorse or promote products derived
//    from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
// OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "MDLBuilder.h"
#include <dp/util/File.h>
#include <algorithm>
#include <cctype>

static std::string extractFunctionName(std::string const& call)
{
  // the function name is the last part, after the last ':', before any opening brace '('
  size_t bracePos = call.find_first_of('(');
  size_t startPos = call.find_last_of(':', bracePos);
  return call.substr(startPos + 1, bracePos - startPos - 1);
}

static std::string extractNameSpace(std::string const& name)
{
  // all names with namespace should start with "::" or with "mdl::"
  if ((name.substr(0, 2) == "::") || (name.substr(0, 5) == "mdl::"))
  {
    // -> use stuff from right after the first "::" to right before the last "::"
    size_t bracePos = name.find_first_of('(');
    size_t startPos = name.find_first_of(':');
    size_t endPos = name.find_last_of(':', bracePos);
    DP_ASSERT((startPos != std::string::npos) && (endPos != std::string::npos) && (startPos < endPos));
    size_t count = endPos - startPos - 3;
    if (endPos < count)
    {
      count = 0;
    }
    return name.substr(startPos + 2, count);
  }
  else
  {
    // all other names should not hold any ':'
    DP_ASSERT(name.find_first_of(':') == std::string::npos);
  }
  return "";
}

static std::string extractTypeName(std::string const& type)
{
  // first make the name lower case
  std::string typeName;
  std::transform(type.begin(), type.end(), std::back_inserter(typeName), std::tolower);

  // then filter out any namespaces -> start right of the last ':'
  size_t pos = typeName.find_last_of(':');
  if (pos != std::string::npos)
  {
    typeName = typeName.substr(pos + 1);
  }

  // translate vector and matrix sizes like <2> and <4,4> to simple size extensions like 2 and 4x4
  pos = typeName.find('<');
  if (pos != std::string::npos)
  {
    size_t commaPos = typeName.find(',', pos);
    if (commaPos != std::string::npos)
    {
      typeName = typeName.substr(0, pos) + typeName.substr(pos + 1, commaPos - pos - 1) + "x" + typeName.substr(commaPos + 1, typeName.length() - commaPos - 2);
    }
    else
    {
      typeName = typeName.substr(0, pos) + typeName.substr(pos + 1, typeName.length() - pos - 2);
    }
  }

  return typeName;
}

static void tokenizeType(std::string const& type, std::string & nameSpace, std::string & name)
{
  nameSpace = extractNameSpace(type);
  name = extractTypeName(type);
}

template <typename DestType, typename SrcType>
static std::unique_ptr<DestType> uniqueCast(std::unique_ptr<SrcType> &src)
{
  DP_ASSERT(src && dynamic_cast<DestType*>(src.get()));
  DestType * destPtr = static_cast<DestType*>(src.get());
  src.release();
  return std::unique_ptr<DestType>(destPtr);
}

MDLBuilder::MDLBuilder()
  : m_currentEnum(nullptr)
  , m_currentMaterial(nullptr)
  , m_currentParameter(nullptr)
{
}

void MDLBuilder::clear()
{
  DP_ASSERT(!m_currentEnum && m_currentExpression.empty() && !m_currentMaterial && !m_currentParameter && m_currentStructure.empty());
  m_enums.clear();
  m_imports.clear();
  m_materials.clear();
  m_structures.clear();
}

std::map<std::string,std::set<std::string>> const& MDLBuilder::getImports() const
{
  return m_imports;
}

std::vector<MDLBuilder::MaterialData> const& MDLBuilder::getMaterials() const
{
  return m_materials;
}

bool MDLBuilder::annotationBegin( std::string const& name, std::vector<std::pair<std::string, std::string>> const& arguments)
{
  std::string callSpace = extractNameSpace(name);
  std::string callName = extractFunctionName(name);
  registerImport(callSpace, callName);

  std::vector<ArgumentData> callArguments;
  callArguments.reserve(arguments.size());
  for (auto const& arg : arguments)
  {
    std::string argSpace = extractNameSpace(arg.first);
    std::string argName = extractTypeName(arg.first);
    registerImport(argSpace, argName);
    callArguments.push_back(ArgumentData(argSpace, argName, arg.second));
  }

  m_currentExpression.push(std::make_unique<ExpressionDataCall>("", "void", callSpace, callName, callArguments));

  return true;
}

void MDLBuilder::annotationEnd()
{
  DP_ASSERT(!m_currentExpression.empty());
  DP_ASSERT(dynamic_cast<ExpressionDataCall*>(m_currentExpression.top().get()));

  auto annotations = m_currentParameter ? &m_currentParameter->annotations : &m_currentMaterial->annotations;
  annotations->push_back(uniqueCast<ExpressionDataCall>(m_currentExpression.top()));
  m_currentExpression.pop();
}

bool MDLBuilder::argumentBegin( size_t idx )
{
#if !defined(NDEBUG)
  DP_ASSERT(!m_currentExpression.empty() && dynamic_cast<ExpressionDataCall*>(m_currentExpression.top().get()));
  ExpressionDataCall* call = static_cast<ExpressionDataCall*>(m_currentExpression.top().get());
  DP_ASSERT((idx < call->argumentData.size()) && (call->arguments.find(idx) == call->arguments.end()));
#endif

  m_currentIndex.push(idx);
  return true;
}

void MDLBuilder::argumentEnd()
{
  DP_ASSERT(2 <= m_currentExpression.size() && !m_currentIndex.empty());
  std::unique_ptr<ExpressionData> argument = std::move(m_currentExpression.top());
  m_currentExpression.pop();

  DP_ASSERT(dynamic_cast<ExpressionDataCall*>(m_currentExpression.top().get()));
  ExpressionDataCall* call = static_cast<ExpressionDataCall*>(m_currentExpression.top().get());
  DP_ASSERT(call->arguments.find(m_currentIndex.top()) == call->arguments.end());
  DP_ASSERT(isCompatibleType(call->argumentData[m_currentIndex.top()].type, argument.get()));
  call->arguments[m_currentIndex.top()] = std::move(argument);
  m_currentIndex.pop();
}

bool MDLBuilder::arrayBegin( std::string const& type, size_t size )
{
  std::string typeSpace, typeName;
  ::tokenizeType(type, typeSpace, typeName);
  registerType(type);

  m_currentExpression.push(std::make_unique<ExpressionDataArray>(typeSpace, typeName, size));

#if !defined(NDEBUG)
  m_currentVectorSize.push(size);
#endif

  return true;
}

void MDLBuilder::arrayEnd()
{
  DP_ASSERT(!m_currentExpression.empty() && dynamic_cast<ExpressionDataArray*>(m_currentExpression.top().get()));
  DP_ASSERT(!m_currentVectorSize.empty() && (static_cast<ExpressionDataArray*>(m_currentExpression.top().get())->values.size() == m_currentVectorSize.top()));

#if !defined(NDEBUG)
  m_currentVectorSize.pop();
#endif
}

bool MDLBuilder::arrayElementBegin(size_t idx)
{
  DP_ASSERT(!m_currentExpression.empty() && dynamic_cast<ExpressionDataArray*>(m_currentExpression.top().get()));
  DP_ASSERT(idx == static_cast<ExpressionDataArray*>(m_currentExpression.top().get())->values.size());

#if !defined(NDEBUG)
  m_currentIndex.push(idx);
#endif

  return true;
}

void MDLBuilder::arrayElementEnd()
{
  DP_ASSERT(2 <= m_currentExpression.size() && !m_currentIndex.empty());
  std::unique_ptr<ExpressionData> argument = std::move(m_currentExpression.top());
  m_currentExpression.pop();

  DP_ASSERT(dynamic_cast<ExpressionDataArray*>(m_currentExpression.top().get()));
  ExpressionDataArray* array = static_cast<ExpressionDataArray*>(m_currentExpression.top().get());
  DP_ASSERT(array->values.size() == m_currentIndex.top());
  DP_ASSERT(isCompatibleType(array->type, argument.get()));
  array->values.push_back(std::move(argument));

#if !defined(NDEBUG)
  m_currentIndex.pop();
#endif
}

bool MDLBuilder::callBegin( std::string const& type, std::string const& name, std::vector<std::pair<std::string, std::string>> const& arguments)
{
  std::string typeSpace = extractNameSpace(type);
  std::string typeName = extractTypeName(type);
  registerImport(typeSpace, typeName);

  std::string callSpace = extractNameSpace(name);
  std::string callName = extractFunctionName(name);
  registerImport(callSpace, callName);

  std::vector<ArgumentData> callArguments;
  callArguments.reserve(arguments.size());
  for (auto const& arg : arguments)
  {
    std::string argSpace = extractNameSpace(arg.first);
    std::string argName = extractTypeName(arg.first);
    registerImport(argSpace, argName);
    callArguments.push_back(ArgumentData(argSpace, argName, arg.second));
  }

  m_currentExpression.push(std::make_unique<ExpressionDataCall>(typeSpace, typeName, callSpace, callName, callArguments));

  return( true );
}

void MDLBuilder::callEnd()
{
  DP_ASSERT(!m_currentExpression.empty());
  DP_ASSERT(dynamic_cast<ExpressionDataCall*>(m_currentExpression.top().get()));
}

void MDLBuilder::defaultRef( std::string const& type )
{
  DP_ASSERT(type == "texture_2d");
  m_currentExpression.push(std::make_unique<ExpressionDataConstant>(std::make_unique<ValueDataTexture>()));
}

bool MDLBuilder::enumTypeBegin( std::string const& name, size_t size )
{
  DP_ASSERT(!m_currentEnum);
  std::string enumName = registerType(name);

  if (m_enums.find(enumName) == m_enums.end())
  {
    m_currentEnum = &m_enums.insert(std::make_pair(enumName, EnumData(enumName))).first->second;
  }

  return !!m_currentEnum;    // if we didn't encounter that before, get it
}

void MDLBuilder::enumTypeEnd()
{
  DP_ASSERT(m_currentEnum);
  m_currentEnum = nullptr;
}

void MDLBuilder::enumTypeValue( std::string const& name, int value )
{
  DP_ASSERT(m_currentEnum && m_currentEnum->elements.find(value) == m_currentEnum->elements.end());
  m_currentEnum->elements[value] = name;
}

bool MDLBuilder::fieldBegin( std::string const& name )
{
  if (name == "backface")
  {
    int a = 0;
  }
  DP_ASSERT(m_currentMaterial && (m_currentMaterial->fields.find(name) == m_currentMaterial->fields.end()));
  m_currentField = m_currentMaterial->fields.insert(std::make_pair(name, std::unique_ptr<ExpressionData>())).first;
  return( true );
}

void MDLBuilder::fieldEnd()
{
  DP_ASSERT(m_currentMaterial && (m_currentField != m_currentMaterial->fields.end()) && (m_currentExpression.size() == 1));
  m_currentField->second.swap(m_currentExpression.top());
  m_currentExpression.pop();
  m_currentField = m_currentMaterial->fields.end();
}

bool MDLBuilder::fileBegin( std::string const& name )
{
  m_enums.clear();
  m_imports.clear();
  m_materials.clear();
  m_structures.clear();

#if !defined(NDEBUG)
  size_t dotPos = name.find_last_of('.');
  DP_ASSERT(dotPos != std::string::npos);
  DP_ASSERT(name.substr(dotPos, 4) == ".mdl");
  size_t startPos = name.find_last_of('\\');
  DP_ASSERT((startPos != std::string::npos) && (startPos < dotPos));
  m_fileName = name.substr(startPos + 1, dotPos - startPos - 1);
#endif

  return true;
}

void MDLBuilder::fileEnd()
{
}

bool MDLBuilder::materialBegin( std::string const& name, dp::math::Vec4ui const& hash )
{
  DP_ASSERT(!m_currentMaterial);
  DP_ASSERT(name.substr(0, 5) == "mdl::");
  size_t endPos = name.find_last_of(':');
  DP_ASSERT(endPos != std::string::npos);
  size_t startPos = name.find_last_of(':', endPos - 2);
  DP_ASSERT(startPos != std::string::npos);
#if !defined(NDEBUG)
  DP_ASSERT(name.substr(startPos + 1, endPos - startPos - 2) == m_fileName);
#endif

  std::string materialName = name.substr(endPos + 1);
#if !defined(NDEBUG)
  for (MaterialData const& md : m_materials)
  {
    DP_ASSERT(md.name != materialName);
  }
  // possible improvement: if we have a material with the same hash, just get their parammeters and use the rest from that material !
#endif

  m_materials.push_back(MaterialData(materialName, hash));
  m_currentMaterial = &m_materials.back();

  return true;
}

void MDLBuilder::materialEnd()
{
  DP_ASSERT(m_currentMaterial);
  m_currentMaterial = nullptr;
}

bool MDLBuilder::matrixBegin( std::string const& type )
{
  DP_ASSERT((type.length() == 8) && (type.substr(0, 5) == "float") && std::isdigit(type[5]) && (type[6] == 'x') && std::isdigit(type[7]));
  m_currentExpression.push(std::make_unique<ExpressionDataConstant>(std::make_unique<ValueDataMatrix<float>>()));

#if !defined(NDEBUG)
  m_currentVectorSize.push(type[5] - '0');
#endif

  return true;
}

bool MDLBuilder::matrixElementBegin(size_t idx)
{
  DP_ASSERT(!m_currentExpression.empty() && dynamic_cast<ExpressionDataConstant*>(m_currentExpression.top().get())
    && dynamic_cast<ValueDataMatrix<float>*>(static_cast<ExpressionDataConstant*>(m_currentExpression.top().get())->value.get()));
  DP_ASSERT(idx == static_cast<ValueDataMatrix<float>*>(static_cast<ExpressionDataConstant*>(m_currentExpression.top().get())->value.get())->values.size());

#if !defined(NDEBUG)
  m_currentIndex.push(idx);
#endif

  return true;
}

void MDLBuilder::matrixElementEnd()
{
  DP_ASSERT(2 <= m_currentExpression.size() && !m_currentIndex.empty());
  std::unique_ptr<ExpressionData> argument = std::move(m_currentExpression.top());
  DP_ASSERT(dynamic_cast<ExpressionDataConstant*>(argument.get()) && dynamic_cast<ValueDataVector<float>*>(static_cast<ExpressionDataConstant*>(argument.get())->value.get()));
  m_currentExpression.pop();

  DP_ASSERT(dynamic_cast<ExpressionDataConstant*>(m_currentExpression.top().get()));
  ExpressionDataConstant* constant = static_cast<ExpressionDataConstant*>(m_currentExpression.top().get());
  DP_ASSERT(dynamic_cast<ValueDataMatrix<float>*>(constant->value.get()));
  ValueDataMatrix<float>* matrix = static_cast<ValueDataMatrix<float>*>(constant->value.get());
  DP_ASSERT(matrix->values.size() == m_currentIndex.top());
  matrix->values.push_back(uniqueCast<ValueDataVector<float>>(static_cast<ExpressionDataConstant*>(argument.get())->value));

#if !defined(NDEBUG)
  m_currentIndex.pop();
#endif
}

void MDLBuilder::matrixEnd()
{
  DP_ASSERT(!m_currentExpression.empty() && dynamic_cast<ExpressionDataConstant*>(m_currentExpression.top().get())
    && dynamic_cast<ValueDataMatrix<float>*>(static_cast<ExpressionDataConstant*>(m_currentExpression.top().get())->value.get()));
  DP_ASSERT(!m_currentVectorSize.empty() && (static_cast<ValueDataMatrix<float>*>(static_cast<ExpressionDataConstant*>(m_currentExpression.top().get())->value.get())->values.size() == m_currentVectorSize.top()));

#if !defined(NDEBUG)
  m_currentVectorSize.pop();
#endif
}

bool MDLBuilder::parameterBegin( unsigned int index, std::string const& modifier, std::string const& type, std::string const& name )
{
  DP_ASSERT(!m_currentParameter);
  DP_ASSERT(m_currentMaterial->parameters.size() == index);
#if !defined(NDEBUG)
  for (ParameterData const& pd : m_currentMaterial->parameters)
  {
    DP_ASSERT(pd.name != name);
  }
#endif

  m_currentMaterial->parameters.push_back(ParameterData(modifier, registerType(type), name));
  m_currentParameter = &m_currentMaterial->parameters.back();

  return true;
}

void MDLBuilder::parameterEnd()
{
  DP_ASSERT(m_currentParameter && (m_currentExpression.size() == 1));
  DP_ASSERT(isCompatibleType(m_currentParameter->type, m_currentExpression.top().get()));

  m_currentParameter->value = std::move(m_currentExpression.top());
  m_currentExpression.pop();
  m_currentParameter = nullptr;
}

void MDLBuilder::referenceParameter( unsigned int idx )
{
  m_currentExpression.push(std::make_unique<ExpressionDataParameter>(idx));
}

void MDLBuilder::referenceTemporary( unsigned int idx )
{
  m_currentExpression.push(std::make_unique<ExpressionDataTemporary>(idx));
}

bool MDLBuilder::structureBegin( std::string const& name)
{
  DP_ASSERT(m_structures.find(name) != m_structures.end());
  StructureData const& sd = m_structures.find(name)->second;

  m_currentExpression.push(std::make_unique<ExpressionDataCall>(sd.nameSpace, sd.name, sd.nameSpace, sd.name, sd.elements));

  return(true);
}

void MDLBuilder::structureEnd()
{
  DP_ASSERT(!m_currentExpression.empty());
  DP_ASSERT(dynamic_cast<ExpressionDataCall*>(m_currentExpression.top().get()));
}

bool MDLBuilder::structureMemberBegin(unsigned int idx)
{
  DP_ASSERT(!m_currentExpression.empty() && dynamic_cast<ExpressionDataCall*>(m_currentExpression.top().get()));
  DP_ASSERT(idx < static_cast<ExpressionDataCall*>(m_currentExpression.top().get())->argumentData.size());

  m_currentIndex.push(idx);

  return true;
}

void MDLBuilder::structureMemberEnd()
{
  DP_ASSERT(2 <= m_currentExpression.size() && !m_currentIndex.empty());
  std::unique_ptr<ExpressionData> argument = std::move(m_currentExpression.top());
  m_currentExpression.pop();

  DP_ASSERT(dynamic_cast<ExpressionDataCall*>(m_currentExpression.top().get()));
  ExpressionDataCall* call = static_cast<ExpressionDataCall*>(m_currentExpression.top().get());
  DP_ASSERT(call->arguments.find(m_currentIndex.top()) == call->arguments.end());
  DP_ASSERT(isCompatibleType(call->argumentData[m_currentIndex.top()].type, argument.get()));
  call->arguments[m_currentIndex.top()] = std::move(argument);
  m_currentIndex.pop();
}

bool MDLBuilder::structureTypeBegin( std::string const& name )
{
  std::string nameSpace, typeName;
  ::tokenizeType(name, nameSpace, typeName);
  registerImport(nameSpace, typeName);

  bool structureUnknown = (m_structures.find(name) == m_structures.end());
  if (structureUnknown)
  {
    m_currentStructure.push(&m_structures.insert(std::make_pair(name,StructureData(nameSpace, typeName))).first->second);
  }

  return structureUnknown;    // if we didn't encounter that before, get it
}

void MDLBuilder::structureTypeElement( std::string const& type, std::string const& name )
{
  DP_ASSERT(!m_currentStructure.empty());

  std::string typeSpace = extractNameSpace(type);
  std::string typeName = extractTypeName(type);
  registerImport(typeSpace, typeName);
  m_currentStructure.top()->elements.push_back(ArgumentData(typeSpace, typeName, name));
}

void MDLBuilder::structureTypeEnd()
{
  DP_ASSERT(!m_currentStructure.empty());
  m_currentStructure.pop();
}

bool MDLBuilder::temporaryBegin( unsigned int idx )
{
  DP_ASSERT(m_currentMaterial && (m_currentMaterial->temporaries.size() == idx) && m_currentExpression.empty());

  return true;
}

void MDLBuilder::temporaryEnd()
{
  DP_ASSERT(m_currentMaterial && (m_currentExpression.size() == 1));
  m_currentMaterial->temporaries.push_back(std::move(m_currentExpression.top()));
  m_currentExpression.pop();
}

void MDLBuilder::valueBool( bool value )
{
  m_currentExpression.push(std::make_unique<ExpressionDataConstant>(std::make_unique<ValueDataBool>(value)));
}

void MDLBuilder::valueBsdfMeasurement( std::string const& value )
{
  DP_ASSERT(!"never passed this path");
}

void MDLBuilder::valueColor( dp::math::Vec3f const& value )
{
  m_currentExpression.push(std::make_unique<ExpressionDataConstant>(std::make_unique<ValueDataColor>(value)));
}

void MDLBuilder::valueEnum( std::string const& type, int value, std::string const& name )
{
  std::string typeSpace, typeName;
  ::tokenizeType(type, typeSpace, typeName);
  registerType(type);
  m_currentExpression.push(std::make_unique<ExpressionDataConstant>(std::make_unique<ValueDataEnum>(typeSpace, typeName, name)));
}

void MDLBuilder::valueFloat( float value )
{
  m_currentExpression.push(std::make_unique<ExpressionDataConstant>(std::make_unique<ValueDataFloat>(value)));
}

void MDLBuilder::valueInt( int value )
{
  m_currentExpression.push(std::make_unique<ExpressionDataConstant>(std::make_unique<ValueDataInt>(value)));
}

void MDLBuilder::valueLightProfile( std::string const& value )
{
  DP_ASSERT(!"never passed this path");
}

void MDLBuilder::valueString( std::string const& value )
{
  m_currentExpression.push(std::make_unique<ExpressionDataConstant>(std::make_unique<ValueDataString>(value)));
}

void MDLBuilder::valueTexture( std::string const& name, GammaMode gamma )
{
  registerImport("tex", "gamma_mode");
  m_currentExpression.push(std::make_unique<ExpressionDataConstant>(std::make_unique<ValueDataTexture>(name, gamma)));
}

bool MDLBuilder::vectorBegin( std::string const& type )
{
  DP_ASSERT((type.length() == 6) && (type.substr(0, 5) == "float") && std::isdigit(type[5]));
  m_currentExpression.push(std::make_unique<ExpressionDataConstant>(std::make_unique<ValueDataVector<float>>()));

#if !defined(NDEBUG)
  m_currentVectorSize.push(type[5] - '0');
#endif

  return true;
}

bool MDLBuilder::vectorElementBegin(size_t idx)
{
  DP_ASSERT(!m_currentExpression.empty() && dynamic_cast<ExpressionDataConstant*>(m_currentExpression.top().get())
    && dynamic_cast<ValueDataVector<float>*>(static_cast<ExpressionDataConstant*>(m_currentExpression.top().get())->value.get()));
  DP_ASSERT(idx == static_cast<ValueDataVector<float>*>(static_cast<ExpressionDataConstant*>(m_currentExpression.top().get())->value.get())->values.size());

#if !defined(NDEBUG)
  m_currentIndex.push(idx);
#endif

  return true;
}

void MDLBuilder::vectorElementEnd()
{
  DP_ASSERT(2 <= m_currentExpression.size() && !m_currentIndex.empty());
  std::unique_ptr<ExpressionData> argument = std::move(m_currentExpression.top());
  DP_ASSERT(dynamic_cast<ExpressionDataConstant*>(argument.get()) && dynamic_cast<ValueDataFloat*>(static_cast<ExpressionDataConstant*>(argument.get())->value.get()));
  m_currentExpression.pop();

  DP_ASSERT(dynamic_cast<ExpressionDataConstant*>(m_currentExpression.top().get()));
  ExpressionDataConstant* constant = static_cast<ExpressionDataConstant*>(m_currentExpression.top().get());
  DP_ASSERT(dynamic_cast<ValueDataVector<float>*>(constant->value.get()));
  ValueDataVector<float>* vector = static_cast<ValueDataVector<float>*>(constant->value.get());
  DP_ASSERT(vector->values.size() == m_currentIndex.top());
  vector->values.push_back(static_cast<ValueDataFloat*>(static_cast<ExpressionDataConstant*>(argument.get())->value.get())->value);

#if !defined(NDEBUG)
  m_currentIndex.pop();
#endif
}

void MDLBuilder::vectorEnd()
{
  DP_ASSERT(!m_currentExpression.empty() && dynamic_cast<ExpressionDataConstant*>(m_currentExpression.top().get())
    && dynamic_cast<ValueDataVector<float>*>(static_cast<ExpressionDataConstant*>(m_currentExpression.top().get())->value.get()));
  DP_ASSERT(!m_currentVectorSize.empty() && (static_cast<ValueDataVector<float>*>(static_cast<ExpressionDataConstant*>(m_currentExpression.top().get())->value.get())->values.size() == m_currentVectorSize.top()));

#if !defined(NDEBUG)
  m_currentVectorSize.pop();
#endif
}

void MDLBuilder::registerImport(std::string const& nameSpace, std::string const& name)
{
  if (!nameSpace.empty() && (name.substr(0, 8) != "operator"))
  {
    m_imports[nameSpace].insert(name);
  }
}

std::string MDLBuilder::registerType(std::string const& name)
{
  static std::set<std::string> standardTypes = { "bool", "color", "float", "float2", "float3", "int", "string", "texture_2d" };

  std::string typeName = name;
  size_t endPos = typeName.find_first_of('[');
  if (endPos != std::string::npos)
  {
    typeName = typeName.substr(0, endPos);
  }

  std::set<std::string>::const_iterator standardIt = standardTypes.find(typeName);
  if (standardIt == standardTypes.end())
  {
    std::string nameSpace = extractNameSpace(typeName);

    size_t startPos = typeName.find_last_of(':');
    DP_ASSERT(startPos != std::string::npos);
    typeName = typeName.substr(startPos + 1);

    if (!nameSpace.empty())
    {
      m_imports[nameSpace].insert(typeName);
    }
  }

  return typeName;
}

#if !defined(NDEBUG)
bool MDLBuilder::isCompatibleType(std::string const& type, MDLBuilder::ExpressionData const* data) const
{
  bool ret = false;
  if (dynamic_cast<MDLBuilder::ExpressionDataArray const*>(data))
  {
    size_t bracePos = type.find('[');
    DP_ASSERT((bracePos == std::string::npos) || (type.back() == ']'));
    ret = (type.substr(0, bracePos) == static_cast<MDLBuilder::ExpressionDataArray const*>(data)->type);
  }
  else if (dynamic_cast<MDLBuilder::ExpressionDataCall const*>(data))
  {
    ret = (static_cast<MDLBuilder::ExpressionDataCall const*>(data)->type == type);
  }
  else if (dynamic_cast<MDLBuilder::ExpressionDataConstant const*>(data))
  {
    MDLBuilder::ExpressionDataConstant const* constant = static_cast<MDLBuilder::ExpressionDataConstant const*>(data);
    if (dynamic_cast<MDLBuilder::ValueDataBool const*>(constant->value.get()))
    {
      ret = (type == "bool");
    }
    else if (dynamic_cast<MDLBuilder::ValueDataColor const*>(constant->value.get()))
    {
      ret = (type == "color");
    }
    else if (dynamic_cast<MDLBuilder::ValueDataEnum const*>(constant->value.get()))
    {
      ret = (type == static_cast<MDLBuilder::ValueDataEnum const*>(constant->value.get())->type);
    }
    else if (dynamic_cast<MDLBuilder::ValueDataFloat const*>(constant->value.get()))
    {
      ret = (type == "float");
    }
    else if (dynamic_cast<MDLBuilder::ValueDataInt const*>(constant->value.get()))
    {
      ret = (type == "int");
    }
    else if (dynamic_cast<MDLBuilder::ValueDataMatrix<float> const*>(constant->value.get()))
    {
      MDLBuilder::ValueDataMatrix<float> const* matrix = static_cast<MDLBuilder::ValueDataMatrix<float> const*>(constant->value.get());
      ret = (type.length() == 8) && (type.substr(0, 5) == "float") && std::isdigit(type[5]) && (type[5] - '0' == matrix->values.size()) && (type[6] == 'x') && std::isdigit(type[7]);
      for (size_t i = 0; i < matrix->values.size() && ret; i++)
      {
        DP_ASSERT(dynamic_cast<MDLBuilder::ValueDataVector<float> const*>(matrix->values[i].get()));
        ret = (type[7] - '0' == static_cast<MDLBuilder::ValueDataVector<float> const*>(matrix->values[i].get())->values.size());
      }
    }
    else if (dynamic_cast<MDLBuilder::ValueDataString const*>(constant->value.get()))
    {
      ret = (type == "string");
    }
    else if (dynamic_cast<MDLBuilder::ValueDataTexture const*>(constant->value.get()))
    {
      ret = (type == "texture_2d");
    }
    else if (dynamic_cast<MDLBuilder::ValueDataVector<float> const*>(constant->value.get()))
    {
      ret = (type.length() == 6) && (type.substr(0, 5) == "float") && std::isdigit(type[5]) && (type[5] - '0' == static_cast<MDLBuilder::ValueDataVector<float> const*>(constant->value.get())->values.size());
    }
    else
    {
      DP_ASSERT(false);
    }
  }
  else if (dynamic_cast<MDLBuilder::ExpressionDataParameter const*>(data))
  {
    MDLBuilder::ExpressionDataParameter const* parameter = static_cast<MDLBuilder::ExpressionDataParameter const*>(data);
    ret = (type == m_currentMaterial->parameters[parameter->index].type);
  }
  else if (dynamic_cast<MDLBuilder::ExpressionDataTemporary const*>(data))
  {
    MDLBuilder::ExpressionDataTemporary const* temporary = static_cast<MDLBuilder::ExpressionDataTemporary const*>(data);
    return isCompatibleType(type, m_currentMaterial->temporaries[temporary->index].get());
  }
  else
  {
    DP_ASSERT(false);
  }
  return ret;
}
#endif
