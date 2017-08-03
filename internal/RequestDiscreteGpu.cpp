
extern "C"
{
    // from http://developer.download.nvidia.com/devzone/devcenter/gamegraphics/files/OptimusRenderingPolicies.pdf
    __declspec(dllexport) int NvOptimusEnablement = 0x00000001;
    // from https://community.amd.com/thread/169965
    __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}