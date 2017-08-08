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

#pragma once

#include <dp/fx/mdl/inc/MDLTokenizer.h>

class MDLBuilder : public dp::fx::mdl::MDLTokenizer
{
  public:
    struct ValueData
    {
      virtual ~ValueData() {}   // make ValueData a base class
    };

    struct ValueDataBool : public ValueData
    {
      ValueDataBool(bool v)
        : value(v)
      {}

      bool value;
    };

    struct ValueDataColor : public ValueData
    {
      ValueDataColor(dp::math::Vec3f const& v)
        : value(v)
      {}

      dp::math::Vec3f value;
    };

    struct ValueDataEnum : public ValueData
    {
      ValueDataEnum(std::string const& ts, std::string const& t, std::string const& v)
        : typeSpace(ts)
        , type(t)
        , value(v)
      {}

      std::string typeSpace;
      std::string type;
      std::string value;
    };

    struct ValueDataFloat : public ValueData
    {
      ValueDataFloat(float v)
        : value(v)
      {}

      float value;
    };

    struct ValueDataInt : public ValueData
    {
      ValueDataInt(int v)
        : value(v)
      {}

      int value;
    };

    struct ValueDataString : public ValueData
    {
      ValueDataString(std::string const& v)
        : value(v)
      {}

      std::string value;
    };

    struct ValueDataTexture : public ValueData
    {
      ValueDataTexture()
        : gamma(GammaMode::DEFAULT)
      {}

      ValueDataTexture(std::string const& n, GammaMode g)
        : name(n)
        , gamma(g)
      {}

      std::string name;
      GammaMode   gamma;
    };

    template <typename Type>
    struct ValueDataVector : public ValueData
    {
      std::vector<Type> values;
    };

    template <typename Type>
    struct ValueDataMatrix : public ValueData
    {
      std::vector<std::unique_ptr<ValueDataVector<Type>>> values;
    };

    struct AnnotationData
    {
      AnnotationData(std::string const& n, std::vector<std::string> const& argTypes)
        : name(n)
      {}

      std::string                             name;
      std::vector<std::unique_ptr<ValueData>> values;
    };

    struct EnumData
    {
      EnumData(std::string const& n)
        : name(n)
      {}

      std::string                   name;
      std::map<size_t, std::string> elements;
    };

    struct ExpressionData
    {
      virtual ~ExpressionData() {}
    };

    struct ArgumentData
    {
      ArgumentData(std::string const& ts, std::string const& t, std::string const& n)
        : typeSpace(ts)
        , type(t)
        , name(n)
      {}

      std::string typeSpace;
      std::string type;
      std::string name;
    };

    struct ExpressionDataArray : public ExpressionData
    {
      ExpressionDataArray(std::string const& ts, std::string const& t, size_t size)
        : typeSpace(ts)
        , type(t)
      {
        values.reserve(size);
      }

      std::string                                   typeSpace;
      std::string                                   type;
      std::vector<std::unique_ptr<ExpressionData>>  values;
    };

    struct ExpressionDataCall : public ExpressionData
    {
      ExpressionDataCall(std::string const& ts, std::string const& t, std::string const& cs, std::string const& c, std::vector<ArgumentData> const& ad)
        : typeNameSpace(ts)
        , type(t)
        , callNameSpace(cs)
        , call(c)
        , argumentData(ad)
      {}

      std::string                                       typeNameSpace;
      std::string                                       type;
      std::string                                       callNameSpace;
      std::string                                       call;
      std::vector<ArgumentData>                         argumentData;
      std::map<size_t,std::unique_ptr<ExpressionData>>  arguments;
    };

    struct ExpressionDataConstant : public ExpressionData
    {
      ExpressionDataConstant(std::unique_ptr<ValueData> v)
      {
        value.reset(v.get());
        v.release();
      }

      std::unique_ptr<ValueData>  value;
    };

    struct ExpressionDataParameter : public ExpressionData
    {
      ExpressionDataParameter(unsigned int idx)
        : index(idx)
      {}

      unsigned int index;
    };

    struct ExpressionDataTemporary : public ExpressionData
    {
      ExpressionDataTemporary(unsigned int idx)
        : index(idx)
      {}

      unsigned int index;
    };

    struct ParameterData
    {
      ParameterData(std::string const& m, std::string const& t, std::string const& n)
        : modifier(m)
        , type(t)
        , name(n)
      {}

      std::string                                       modifier;
      std::string                                       type;
      std::string                                       name;
      std::unique_ptr<ExpressionData>                   value;
      std::vector<std::unique_ptr<ExpressionDataCall>>  annotations;
    };

    struct MaterialData
    {
      MaterialData(std::string const& n, dp::math::Vec4ui const& h)
        : hash(h)
        , name(n)
      {}

      dp::math::Vec4ui                                        hash;
      std::string                                             name;
      std::vector<ParameterData>                              parameters;
      std::vector<std::unique_ptr<ExpressionDataCall>>        annotations;
      std::vector<std::unique_ptr<ExpressionData>>            temporaries;
      std::map<std::string, std::unique_ptr<ExpressionData>>  fields;
    };

    struct StructureData
    {
      StructureData(std::string const& ns, std::string const& n)
        : nameSpace(ns)
        , name(n)
      {}

      std::string               nameSpace;
      std::string               name;
      std::vector<ArgumentData> elements;
    };

  public:
    MDLBuilder();

    void clear();
    std::map<std::string,std::set<std::string>> const& getImports() const;
    std::vector<MaterialData> const& getMaterials() const;

  protected:
    bool annotationBegin( std::string const& name, std::vector<std::pair<std::string, std::string>> const& arguments) override;
    void annotationEnd() override;
    bool argumentBegin( size_t idx ) override;
    void argumentEnd() override;
    bool arrayBegin( std::string const& type, size_t size ) override;
    void arrayEnd() override;
    bool arrayElementBegin(size_t idx) override;
    void arrayElementEnd() override;
    bool callBegin( std::string const& type, std::string const& name, std::vector<std::pair<std::string, std::string>> const& arguments2) override;
    void callEnd() override;
    void defaultRef( std::string const& type ) override;
    bool enumTypeBegin( std::string const& name, size_t size ) override;
    void enumTypeEnd() override;
    void enumTypeValue( std::string const& name, int value ) override;
    bool fieldBegin( std::string const& name ) override;
    void fieldEnd() override;
    bool fileBegin( std::string const& name ) override;
    void fileEnd() override;
    bool materialBegin( std::string const& name, dp::math::Vec4ui const& hash ) override;
    void materialEnd() override;
    bool matrixBegin( std::string const& type ) override;
    bool matrixElementBegin(size_t idx) override;
    void matrixElementEnd() override;
    void matrixEnd() override;
    bool parameterBegin( unsigned int index, std::string const& modifier, std::string const& type, std::string const& name ) override;
    void parameterEnd() override;
    void referenceParameter( unsigned int idx ) override;
    void referenceTemporary( unsigned int idx ) override;
    bool structureBegin(std::string const& name) override;
    void structureEnd() override;
    bool structureMemberBegin(unsigned int idx) override;
    void structureMemberEnd() override;
    bool structureTypeBegin( std::string const& name ) override;
    void structureTypeElement( std::string const& type, std::string const& name ) override;
    void structureTypeEnd() override;
    bool temporaryBegin( unsigned int idx ) override;
    void temporaryEnd() override;
    void valueBool( bool value ) override;
    void valueBsdfMeasurement( std::string const& value ) override;
    void valueColor( dp::math::Vec3f const& value ) override;
    void valueEnum( std::string const& type, int value, std::string const& name ) override;
    void valueFloat( float value ) override;
    void valueInt( int value ) override;
    void valueLightProfile( std::string const& value ) override;
    void valueString( std::string const& value ) override;
    void valueTexture( std::string const& name, GammaMode gamma ) override;
    bool vectorBegin( std::string const& type ) override;
    bool vectorElementBegin(size_t idx) override;
    void vectorElementEnd() override;
    void vectorEnd() override;

  private:
    void registerImport(std::string const& nameSpace, std::string const& typeName);
    std::string registerType(std::string const& name);
#if !defined(NDEBUG)
    bool isCompatibleType(std::string const& type, MDLBuilder::ExpressionData const* data) const;
#endif

  private:
    EnumData                                                          * m_currentEnum;
    std::stack<std::unique_ptr<ExpressionData>>                         m_currentExpression;
    std::map<std::string, std::unique_ptr<ExpressionData>>::iterator    m_currentField;
    std::stack<size_t>                                                  m_currentIndex;
    MaterialData                                                      * m_currentMaterial;
    ParameterData                                                     * m_currentParameter;
    std::stack<StructureData*>                                          m_currentStructure;
    std::map<std::string, EnumData>                                     m_enums;
    std::map<std::string, std::set<std::string>>                        m_imports;
    std::vector<MaterialData>                                           m_materials;
    std::map<std::string, StructureData>                                m_structures;

#if !defined(NDEBUG)
    std::stack<size_t>  m_currentVectorSize;
    std::string         m_fileName;
#endif
};
