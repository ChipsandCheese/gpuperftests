#!/bin/bash

buildInfoFilename="$@"

echo "Generating build information header"

echo "#ifndef BUILD_INFO_H" > "$buildInfoFilename"
echo "#define BUILD_INFO_H" >> "$buildInfoFilename"
echo "#ifdef __cplusplus" >> "$buildInfoFilename"
echo "extern \"C\" {" >> "$buildInfoFilename"
echo "#endif" >> "$buildInfoFilename"
if [ ! -z "${CI_COMMIT_TAG}" ];
then
    echo "#define BUILD_INFO_TAG \"${CI_COMMIT_TAG}\"" >> "$buildInfoFilename"
fi
if [ ! -z "${CI_COMMIT_SHA}" ];
then
    echo "#define BUILD_INFO_SHA \"${CI_COMMIT_SHA}\"" >> "$buildInfoFilename"
    echo "#define BUILD_INFO_BRANCH \"${CI_COMMIT_BRANCH}\"" >> "$buildInfoFilename"
fi
if [ ! -z "${CI_PIPELINE_IID}" ];
then
    echo "#define BUILD_INFO_IDENTIFIER \"${CI_PIPELINE_IID}\"" >> "$buildInfoFilename"
else
    echo "#define BUILD_INFO_IDENTIFIER \"0\"" >> "$buildInfoFilename"
fi
echo "#ifdef BUILD_INFO_TAG" >> "$buildInfoFilename"
echo "#ifdef _DEBUG" >> "$buildInfoFilename"
echo "#define BUILD_INFO_TYPE_STRING \"Release Build (Debug Configuration)\"" >> "$buildInfoFilename"
echo "#define BUILD_INFO_TYPE_SUFFIX (\"-\" BUILD_INFO_TAG \" (!!! TESTING BUILD OF RELEASE CONFIGURATION !!!)\")" >> "$buildInfoFilename"
echo "#else" >> "$buildInfoFilename"
echo "#define BUILD_INFO_TYPE_STRING \"Release Build\"" >> "$buildInfoFilename"
echo "#define BUILD_INFO_TYPE_SUFFIX \"\"" >> "$buildInfoFilename"
echo "#define BUILD_INFO_TYPE_RELEASE" >> "$buildInfoFilename"
echo "#endif" >> "$buildInfoFilename"
echo "#else" >> "$buildInfoFilename"
echo "#ifdef BUILD_INFO_SHA" >> "$buildInfoFilename"
echo "#define BUILD_INFO_TYPE_STRING \"Internal Build\"" >> "$buildInfoFilename"
echo "#define BUILD_INFO_TYPE_SUFFIX (\"-\" BUILD_INFO_SHA \"-\" BUILD_INFO_BRANCH \" (!!! INTERNAL BUILD !!!)\")" >> "$buildInfoFilename"
echo "#else" >> "$buildInfoFilename"
echo "#define BUILD_INFO_TYPE_STRING \"Local Developer Build\"" >> "$buildInfoFilename"
echo "#define BUILD_INFO_TYPE_SUFFIX \"-LOCAL_BUILD (!!! DEVELOPMENT BUILD !!!)\"" >> "$buildInfoFilename"
echo "#endif" >> "$buildInfoFilename"
echo "#endif" >> "$buildInfoFilename"
echo "#ifdef __cplusplus" >> "$buildInfoFilename"
echo "}" >> "$buildInfoFilename"
echo "#endif" >> "$buildInfoFilename"
echo "#endif" >> "$buildInfoFilename"