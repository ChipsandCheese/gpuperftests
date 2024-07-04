rem Generating build information header
@echo off

set buildInfoFilepath="%*"
echo #ifndef BUILD_INFO_H> %buildInfoFilepath%
echo #define BUILD_INFO_H>> %buildInfoFilepath%
echo #ifdef __cplusplus>> %buildInfoFilepath%
echo extern "C" {>> %buildInfoFilepath%
echo #endif>> %buildInfoFilepath%
IF "%CI_COMMIT_TAG%" == "" GOTO SkipCommitTag
echo #define BUILD_INFO_TAG "%CI_COMMIT_TAG%">> %buildInfoFilepath%
:SkipCommitTag
IF "%CI_COMMIT_SHA%" == "" GOTO SkipCommitSha
echo #define BUILD_INFO_SHA "%CI_COMMIT_SHA%">> %buildInfoFilepath%
echo #define BUILD_INFO_BRANCH "%CI_COMMIT_BRANCH%">> %buildInfoFilepath%
:SkipCommitSha
IF "%CI_PIPELINE_IID%" == "" GOTO PrintEmptyIdentifier
echo #define BUILD_INFO_IDENTIFIER "%CI_PIPELINE_IID%">> %buildInfoFilepath%
GOTO IdentifierPrinted
:PrintEmptyIdentifier
echo #define BUILD_INFO_IDENTIFIER "0">> %buildInfoFilepath%
:IdentifierPrinted
echo #ifdef BUILD_INFO_TAG>> %buildInfoFilepath%
echo #ifdef _DEBUG>> %buildInfoFilepath%
echo #define BUILD_INFO_TYPE_STRING "Release Build (Debug Configuration)">> %buildInfoFilepath%
echo #define BUILD_INFO_TYPE_SUFFIX ("-" BUILD_INFO_TAG " (!!! TESTING BUILD OF RELEASE CONFIGURATION !!!)")>> %buildInfoFilepath%
echo #else>> %buildInfoFilepath%
echo #define BUILD_INFO_TYPE_STRING "Release Build">> %buildInfoFilepath%
echo #define BUILD_INFO_TYPE_SUFFIX "">> %buildInfoFilepath%
echo #define BUILD_INFO_TYPE_RELEASE>> %buildInfoFilepath%
echo #endif>> %buildInfoFilepath%
echo #else>> %buildInfoFilepath%
echo #ifdef BUILD_INFO_SHA>> %buildInfoFilepath%
echo #define BUILD_INFO_TYPE_STRING "Internal Build">> %buildInfoFilepath%
echo #define BUILD_INFO_TYPE_SUFFIX ("-" BUILD_INFO_SHA "-" BUILD_INFO_BRANCH " (!!! INTERNAL BUILD !!!)")>> %buildInfoFilepath%
echo #else>> %buildInfoFilepath%
echo #define BUILD_INFO_TYPE_STRING "Local Developer Build">> %buildInfoFilepath%
echo #define BUILD_INFO_TYPE_SUFFIX "-LOCAL_BUILD (!!! DEVELOPMENT BUILD !!!)">> %buildInfoFilepath%
echo #endif>> %buildInfoFilepath%
echo #endif>> %buildInfoFilepath%
echo #ifdef __cplusplus>> %buildInfoFilepath%
echo }>> %buildInfoFilepath%
echo #endif>> %buildInfoFilepath%
echo #endif>> %buildInfoFilepath%

@echo on