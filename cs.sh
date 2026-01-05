mkdir -p compiledshaders
for shader in shaders/*.vert  shaders/*.comp shaders/*.frag; do
  filename=$(basename "$shader")
  glslc "$shader" -o "compiledshaders/$filename.spv"
done
