
# default values
ACTION_TYPE="generate"
BUILD_TYPE="release"
PLATFORM="linux"

# Parse command line arguments
if [ "$1" != "" ]; then
  ACTION_TYPE="$1"
fi

echo "ACTION TYPE: "
echo "$ACTION_TYPE"

if [ "$2" != "" ]; then
  BUILD_TYPE="$2"
fi

if [ "$3" != "" ]; then
  PLATFORM="$3"
fi

if [ $PLATFORM == "linux" ]; then
  export PATH=$PATH:$THIRDPARTY_PATH/cmake/lin64/bin
elif [ $PLATFORM == "mac" ]; then
  export PATH=$PATH:$THIRDPARTY_PATH/cmake/mac/bin
fi


if [ $ACTION_TYPE == "generate" ]; then
  python3 build/build_wx.py --wx_src_dir=. --platform=$PLATFORM --action=generate
elif [ $ACTION_TYPE == "build" ]; then
  python3 build/build_wx.py --wx_src_dir=. --platform=$PLATFORM --action=build --build_type=$BUILD_TYPE
fi

