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

#include <iostream>
#include <boost/program_options.hpp>
#include <dp/DP.h>
#include <dp/util/File.h>
#include "MDLBuilder.h"

std::string format(MDLBuilder::MaterialData const& material, MDLBuilder::ExpressionData const* data, size_t level);
std::string format(MDLBuilder::MaterialData const& material, MDLBuilder::ExpressionDataCall const* call, size_t level);
std::string format(MDLBuilder::ValueData const* data);


std::string format(MDLBuilder::MaterialData const& material, MDLBuilder::ExpressionData const* data, size_t level)
{
  if (dynamic_cast<MDLBuilder::ExpressionDataArray const*>(data))
  {
    MDLBuilder::ExpressionDataArray const* array = static_cast<MDLBuilder::ExpressionDataArray const*>(data);
    std::ostringstream oss;
    if (!array->typeSpace.empty())
    {
      oss << array->typeSpace << "::";
    }
    oss << array->type << "[](";
    for (size_t i = 0; i < array->values.size(); i++)
    {
      if (i != 0)
      {
        oss << ", ";
      }
      oss << format(material, array->values[i].get(), level);
    }
    oss << ")";
    return oss.str();
  }
  else if (dynamic_cast<MDLBuilder::ExpressionDataCall const*>(data))
  {
    return format(material, static_cast<MDLBuilder::ExpressionDataCall const*>(data), level);
  }
  else if (dynamic_cast<MDLBuilder::ExpressionDataConstant const*>(data))
  {
    return format(static_cast<MDLBuilder::ExpressionDataConstant const*>(data)->value.get());
  }
  else if (dynamic_cast<MDLBuilder::ExpressionDataParameter const*>(data))
  {
    return material.parameters[static_cast<MDLBuilder::ExpressionDataParameter const*>(data)->index].name;
  }
  else if (dynamic_cast<MDLBuilder::ExpressionDataTemporary const*>(data))
  {
    std::ostringstream oss;
    oss << "temporary" << static_cast<MDLBuilder::ExpressionDataTemporary const*>(data)->index;
    return oss.str();
  }
  else
  {
    DP_ASSERT(false);
  }
  return "";
}

bool needsBraces(MDLBuilder::ExpressionData const* data)
{
  return dynamic_cast<MDLBuilder::ExpressionDataCall const*>(data) && (static_cast<MDLBuilder::ExpressionDataCall const*>(data)->call.substr(0, 8) == "operator");
}

std::string indent(size_t level)
{
  std::ostringstream oss;
  for (size_t i = 0; i < level; i++)
  {
    oss << "  ";
  }
  return oss.str();
}

std::string format(MDLBuilder::MaterialData const& material, MDLBuilder::ExpressionDataCall const* call, size_t level)
{
  std::ostringstream oss;
  if (call->call.back() == '@')
  {
    DP_ASSERT(call->arguments.size() == 2);
    auto firstArg = call->arguments.find(0);
    bool firstArgNeedsBraces = needsBraces(firstArg->second.get());
    auto secondArg = call->arguments.find(1);
    oss << (firstArgNeedsBraces ? "(" : "") << format(material, firstArg->second.get(), level) << (firstArgNeedsBraces ? ")" : "")
      << "[" << format(material, secondArg->second.get(), level) << "]";
  }
  else if (call->call.substr(0, 8) == "operator")
  {
    DP_ASSERT(!call->arguments.empty());
    auto firstArg = call->arguments.find(0);
    bool firstArgNeedsBraces = needsBraces(firstArg->second.get());
    switch (call->arguments.size())
    {
      case 1:
        DP_ASSERT((call->call.back() == '-') || (call->call.back() == '!'));
        oss << call->call.back() << (firstArgNeedsBraces ? "(" : "") << format(material, firstArg->second.get(), level) << (firstArgNeedsBraces ? ")" : "");
        break;
      case 2:
        {
          auto secondArg = call->arguments.find(1);
          DP_ASSERT((firstArg != call->arguments.end()) && (secondArg != call->arguments.end()));
          bool secondArgNeedsBraces = needsBraces(secondArg->second.get());
          oss << (firstArgNeedsBraces ? "(" : "") << format(material, firstArg->second.get(), level) << (firstArgNeedsBraces ? ")" : "") << " " << call->call.substr(8) << " "
            << (secondArgNeedsBraces ? "(" : "") << format(material, secondArg->second.get(), level) << (secondArgNeedsBraces ? ")" : "");
        }
        break;
      case 3:
        {
          DP_ASSERT(call->call.back() == '?');
          auto secondArg = call->arguments.find(1);
          auto thirdArg = call->arguments.find(2);
          DP_ASSERT((firstArg != call->arguments.end()) && (secondArg != call->arguments.end()) && (thirdArg != call->arguments.end()));
          bool secondArgNeedsBraces = needsBraces(secondArg->second.get());
          bool thirdArgNeedsBraces = needsBraces(thirdArg->second.get());
          oss << (firstArgNeedsBraces ? "(" : "") << format(material, firstArg->second.get(), level) << (firstArgNeedsBraces ? ")" : "") << " ? "
            << (secondArgNeedsBraces ? "(" : "") << format(material, secondArg->second.get(), level) << (secondArgNeedsBraces ? ")" : "") << " : "
            << (thirdArgNeedsBraces ? "(" : "") << format(material, thirdArg->second.get(), level) << (thirdArgNeedsBraces ? ")" : "");
      }
        break;
      default:
        DP_ASSERT(false);
    }
  }
  else
  {
    size_t dotPos = call->call.find('.');
    if (dotPos != std::string::npos)
    {
      DP_ASSERT(call->arguments.size() == 1);
      auto firstArg = call->arguments.find(0);
      oss << format(material, firstArg->second.get(), level) << call->call.substr(dotPos);
    }
    else
    {
      if (!call->callNameSpace.empty())
      {
        oss << call->callNameSpace << "::";
      }
      oss << call->call;
      if (call->arguments.empty())
      {
        oss << "()";
      }
      else
      {
        oss << std::endl
          << indent(level) << "(" << std::endl;
        bool includeArgumentNames = (call->call.find('[') == std::string::npos);
        for (auto it = call->arguments.begin(); it != call->arguments.end(); ++it)
        {
          DP_ASSERT(includeArgumentNames || (call->argumentData[it->first].name.find_first_not_of("0123456789") == std::string::npos));

          if (it != call->arguments.begin())
          {
            oss << ", " << std::endl;
          }
          oss << indent(level + 1);
          if (includeArgumentNames)
          {
            oss << call->argumentData[it->first].name << " : ";
          }
          oss << format(material, it->second.get(), level + 1);
        }
        oss << std::endl
          << indent(level) << ")";
      }
    }
  }
  std::string tmp = oss.str();
  return oss.str();
}

std::string replaceAll(std::string const& src, std::string const& dst, std::string const& in)
{
  std::string out(in);
  size_t pos = out.find(src);
  while (pos != std::string::npos)
  {
    out.replace(pos, src.length(), dst);
    pos = out.find(src, pos + dst.length());
  }
  return out;
}

std::string stripLeft(std::string const& strip, std::string const& from)
{
  if (from.find(strip) == 0)
  {
    return from.substr(strip.length());
  }
  return from;
}

std::string format(MDLBuilder::ValueData const* data)
{
  std::ostringstream oss;
  oss << std::showpoint;
  if (dynamic_cast<MDLBuilder::ValueDataBool const*>(data))
  {
    oss << (static_cast<MDLBuilder::ValueDataBool const*>(data)->value ? "true" : "false");
  }
  else if (dynamic_cast<MDLBuilder::ValueDataColor const*>(data))
  {
    MDLBuilder::ValueDataColor const* color = static_cast<MDLBuilder::ValueDataColor const*>(data);
    oss << "color(" << color->value[0] << "f, " << color->value[1] << "f, " << color->value[2] << "f)";
  }
  else if (dynamic_cast<MDLBuilder::ValueDataEnum const*>(data))
  {
    MDLBuilder::ValueDataEnum const* Enum = static_cast<MDLBuilder::ValueDataEnum const*>(data);
    if (Enum->typeSpace != "::")
    {
      oss << Enum->typeSpace << "::";
    }
    oss << Enum->value;
  }
  else if (dynamic_cast<MDLBuilder::ValueDataFloat const*>(data))
  {
    oss << static_cast<MDLBuilder::ValueDataFloat const*>(data)->value << "f";
  }
  else if (dynamic_cast<MDLBuilder::ValueDataInt const*>(data))
  {
    oss << static_cast<MDLBuilder::ValueDataInt const*>(data)->value;
  }
  else if (dynamic_cast<MDLBuilder::ValueDataMatrix<float> const*>(data))
  {
    MDLBuilder::ValueDataMatrix<float> const* matrix = static_cast<MDLBuilder::ValueDataMatrix<float> const*>(data);
    oss << "float" << matrix->values.size() << "x" << matrix->values[0]->values.size() << "(";
    for (size_t i = 0; i < matrix->values.size(); i++)
    {
      if (i != 0)
      {
        oss << ", ";
      }
      oss << format(matrix->values[i].get());
    }
    oss << ")";
  }
  else if (dynamic_cast<MDLBuilder::ValueDataString const*>(data))
  {
    oss << "\"" << replaceAll("\"", "\\\"", static_cast<MDLBuilder::ValueDataString const*>(data)->value) << "\"";
  }
  else if (dynamic_cast<MDLBuilder::ValueDataTexture const*>(data))
  {
    static std::string mediaPath = replaceAll("\\", "/", dp::home()) + "/media/effects/mdl/";

    MDLBuilder::ValueDataTexture const* texture = static_cast<MDLBuilder::ValueDataTexture const*>(data);
    oss << "texture_2d(\"" << stripLeft(mediaPath, replaceAll("\\", "/", texture->name)) << "\", tex::gamma_";
    switch (texture->gamma)
    {
      case dp::fx::mdl::MDLTokenizer::GammaMode::DEFAULT:
        oss << "default";
        break;
      case dp::fx::mdl::MDLTokenizer::GammaMode::LINEAR:
        oss << "linear";
        break;
      case dp::fx::mdl::MDLTokenizer::GammaMode::SRGB:
        oss << "srgb";
        break;
    }
    oss << ")";
  }
  else if (dynamic_cast<MDLBuilder::ValueDataVector<float> const*>(data))
  {
    MDLBuilder::ValueDataVector<float> const* vector = static_cast<MDLBuilder::ValueDataVector<float> const*>(data);
    oss << "float" << vector->values.size() << "(";
    for (size_t i = 0; i < vector->values.size(); i++)
    {
      if (i != 0)
      {
        oss << ", ";
      }
      oss << vector->values[i] << "f";
    }
    oss << ")";
  }
  else
  {
    DP_ASSERT(false);
  }
  return oss.str();
}

std::string type(MDLBuilder::MaterialData const& material, MDLBuilder::ExpressionData const* data)
{
  std::string result;
  if (dynamic_cast<MDLBuilder::ExpressionDataCall const*>(data))
  {
    MDLBuilder::ExpressionDataCall const* call = static_cast<MDLBuilder::ExpressionDataCall const*>(data);
    if (!call->typeNameSpace.empty())
    {
      result += call->typeNameSpace + "::";
    }
    result += call->type;
  }
  else if (dynamic_cast<MDLBuilder::ExpressionDataParameter const*>(data))
  {
    result = material.parameters[static_cast<MDLBuilder::ExpressionDataParameter const*>(data)->index].type;
  }
  else
  {
    DP_ASSERT(false);
  }
  return result;
}

int main( int argc, char *argv[] )
{
  boost::program_options::options_description od("Usage: mdl2mdl");
  od.add_options()
    ( "distill", boost::program_options::value<bool>(),        "perform distilling")
    ( "file",    boost::program_options::value<std::string>(), "single file to handle" )
    ( "help",                                                  "show help")
    ( "path",    boost::program_options::value<std::string>(), "path to multiple files to handle" )
    ( "root",    boost::program_options::value<std::string>(), "root path of the material package" )
    ;

  boost::program_options::variables_map opts;
  boost::program_options::store( boost::program_options::parse_command_line( argc, argv, od ), opts );

  if ( !opts["help"].empty() )
  {
    std::cout << od << std::endl;
    return( 0 );
  }
  if ( opts["file"].empty() && opts["path"].empty() )
  {
    std::cout << argv[0] << " : at least argument --file or arguments --path is needed!" << std::endl;
    return( 0 );
  }

  std::string file, root, path;
  if ( !opts["file"].empty() )
  {
    if ( !opts["path"].empty() )
    {
      std::cout << argv[0] << " : argument --file and argument --path exclude each other!" << std::endl;
      return( 0 );
    }
    file = opts["file"].as<std::string>();
  }
  if ( !opts["root"].empty() )
  {
    root = opts["root"].as<std::string>();
    if ( ! dp::util::directoryExists( root ) )
    {
      std::cout << argv[0] << " : root <" << root << "> not found!" << std::endl;
      return( 0 );
    }
    if ( ( root.back() != '\\' ) && ( root.back() != '/' ) )
    {
      root.push_back( '\\' );
    }
  }
  if ( !opts["path"].empty() )
  {
    path = opts["path"].as<std::string>();
  }

  std::vector<std::string> files;
  if ( !file.empty() )
  {
    files.push_back( root + file );
    if ( ! dp::util::fileExists( files.back() ) )
    {
      std::cout << argv[0] << " : file <" << files.back() << "> not found!" << std::endl;
      return( 0 );
    }
    if ( dp::util::getFileExtension( files.back() ) != ".mdl" )
    {
      std::cout << argv[0] << " : file <" << files.back() << "> is not an mdl file!" << std::endl;
      return( 0 );
    }
  }
  else
  {
    path = root + path;
    if ( dp::util::directoryExists( path ) )
    {
      dp::util::findFilesRecursive(".mdl", path, files);
    }
    else if (dp::util::fileExists(path))
    {
      files.push_back(path);
    }
  }

  if ( files.empty() )
  {
    std::cerr << argv[0] << " : No files found!";
    return( -1 );
  }

  dp::util::FileFinder fileFinder;
  if ( !root.empty() )
  {
    DP_ASSERT( ( root.back() == '\\' ) || ( root.back() == '/' ) );
    root.pop_back();
    fileFinder.addSearchPath( root );
  }
  fileFinder.addSearchPath( dp::home() + "/media/effects/mdl" );
  fileFinder.addSearchPath( dp::home() + "/media/textures/mdl" );

  MDLBuilder mdlBuilder;
  mdlBuilder.setFilterDefaults(true);

  for (size_t i = 0; i<files.size(); i++)
  {
    std::cout << "parsing <" << files[i] << ">" << std::endl;

    mdlBuilder.parseFile(files[i], fileFinder);

    std::string filePath = dp::util::getFilePath(files[i]);
    std::string fileStem = dp::util::getFileStem(files[i]);
    std::string fileExtension = dp::util::getFileExtension(files[i]);
    std::string destinationFile = filePath + "\\" + fileStem + "_" + fileExtension;
    std::cout << "writing <" << destinationFile << ">" << std::endl;

    std::ofstream ofs(destinationFile);
    ofs << std::endl << "mdl 1.2;" << std::endl
      << std::endl;

    std::map<std::string, std::set<std::string>> const& imports = mdlBuilder.getImports();
    if (!imports.empty())
    {
      for (auto const& import : imports)
      {
        DP_ASSERT(!import.first.empty());
        if (import.second.size() < 4)
        {
          for (auto const& i : import.second)
          {
            ofs << "import " << import.first << "::" << i << ";" << std::endl;
          }
        }
        else
        {
          ofs << "import " << import.first << "::*;" << std::endl;
        }
      }
      ofs << std::endl;
    }

    std::vector<MDLBuilder::MaterialData> const& materials = mdlBuilder.getMaterials();
    for (auto const& material : materials)
    {
      ofs << "export material " << material.name;
      if (material.parameters.empty())
      {
        ofs << "()" << std::endl;
      }
      else
      {
        ofs << std::endl
          << "(" << std::endl;
        for (size_t p=0 ; p<material.parameters.size() ; p++)
        {
          if (0 < p)
          {
            ofs << "," << std::endl;
          }
          auto const& parameter = material.parameters[p];
          ofs << indent(1);
          if (!parameter.modifier.empty())
          {
            ofs << parameter.modifier << " ";
          }
          ofs << parameter.type << " " << parameter.name;
          if (parameter.value)
          {
            ofs << " = " << format(material, parameter.value.get(), 1);
          }
          ofs << std::endl;
          if (!parameter.annotations.empty())
          {
            ofs << indent(1) << "[[" << std::endl;
            for (size_t a=0 ; a<parameter.annotations.size() ; a++)
            {
              if (0 < a)
              {
                ofs << "," << std::endl;
              }
              ofs << indent(2) << format(material, parameter.annotations[a].get(), 2);
            }
            ofs << std::endl
              << indent(1) << "]]";
          }
        }
        ofs << std::endl
          << ")" << std::endl;
        if (!material.annotations.empty())
        {
          ofs << "[[" << std::endl;
          for (size_t a = 0; a<material.annotations.size(); a++)
          {
            if (0 < a)
            {
              ofs << "," << std::endl;
            }
            ofs << indent(1) << format(material, material.annotations[a].get(), 1);
          }
          ofs << std::endl
            << "]]" << std::endl;
        }
        ofs << "= ";
        if (!material.temporaries.empty())
        {
          ofs << "let" << std::endl
            << "{" << std::endl;
          for (size_t t=0 ; t<material.temporaries.size() ; t++)
          {
            ofs << indent(1) << type(material, material.temporaries[t].get()) << " temporary" << t << " = " << format(material, material.temporaries[t].get(), 1) << ";" << std::endl;
          }
          ofs << "} in ";
        }
        ofs << "material" << std::endl
          << "(" << std::endl;
        for (std::map<std::string, std::unique_ptr<MDLBuilder::ExpressionData>>::const_iterator it = material.fields.begin(); it != material.fields.end(); ++it)
        {
          if (it != material.fields.begin())
          {
            ofs << "," << std::endl;
          }
          ofs << indent(1) << it->first << " : " << format(material, it->second.get(), 1);
        }
        ofs << std::endl
          << ");" << std::endl;
      }

      ofs << std::endl;
    }

    mdlBuilder.clear();
  }

  return( 0 );
}

