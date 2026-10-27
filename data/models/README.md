# Models

Model weights are not committed (D-18): at 46.8 MB, every clone would carry them forever.

| File | Source | Size | SHA-256 |
| --- | --- | --- | --- |
| `resnet18-v2-7.onnx` | [ONNX Model Zoo](https://github.com/onnx/models/raw/main/validated/vision/classification/resnet/model/resnet18-v2-7.onnx) | 46,806,737 B | `3b4d048057854022232aa20bf68262e31718ab7119a17f920b8a2dc1433f824b` |

```sh
curl -L -o data/models/resnet18-v2-7.onnx \
  https://github.com/onnx/models/raw/main/validated/vision/classification/resnet/model/resnet18-v2-7.onnx
sha256sum data/models/resnet18-v2-7.onnx   # must match the table
```

The embedding is the 512-d output of `resnetv22_flatten0_reshape0` (after global average pooling,
before the classifier), verified on OpenCV 5.0.0 with both the classic and the new DNN engine.
