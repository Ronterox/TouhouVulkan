glslcurl="https://storage.googleapis.com/shaderc/badges/build_link_linux_clang_release.html"

url=$(curl -L $glslcurl | grep -oP 'content="\K[^"]*' | awk -F= '{print $2}')
filename=$(basename $url)

if [ -f $filename ]; then
    echo "File already exists. Skipping download..."
else
    echo "Downloading $filename..."
    curl -L -o $filename $url
fi

echo "Extracting $filename..."
tar -xzvf $filename && rm $filename

mv "${filename%.*}" shaderc && make
