#!groovy

// Taken from https://github.com/owncloud/client/blob/master/Jenkinsfile

// Constructed from the DockerFile in admin/linux/DockerFile
def linux = docker.image('dominikschmidt/docker-owncloud-client-linux:latest')
// Constructed from the DockerFile in admin/win/docker/DockerFile
def win32 = docker.image('dominikschmidt/docker-owncloud-client-win32-cross:latest')

node('CLIENT') {
    stage 'Checkout'
        checkout scm
        sh '''git submodule update --init'''

    stage 'Linux - Pull Docker Image'
        linux.pull()

    stage 'Linux - GCC'
        linux.inside {
            sh '''
            export HOME="$(pwd)/home"
            rm -rf build home
            mkdir build
            cd build
            cmake -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=DebugFull ..
            make -j4
            '''
        }

    stage 'Linux - Clang'
        linux.inside {
            sh '''
            export HOME="$(pwd)/home"
            rm -rf build home
            mkdir build
            cd build
            cmake -GNinja -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=DebugFull ..
            ninja -j4
            '''
        }

}
