# Fashion-MNIST Mini + Dropout

Saved model: `fashion-mnist-mini-dropout.keras`  
Source notebook: `fashion-mnist-mini-dropout.ipynb`  
Documented run: November 7, 2026

## Purpose and architecture

A ten-class Fashion-MNIST clothing classifier built as a learning project.

`784 → Dense(128, ReLU) → Dropout(0.2) → Dense(64, ReLU) → Dense(10, softmax)`

Trainable parameters: **109,386**. The model has not been deployed to a Raspberry Pi.

## Input contract

- Raw images: 28 × 28 grayscale pixels, integer values from 0 to 255, using the Fashion-MNIST orientation and intensity convention.
- Model input: NumPy `float32`, shape `(N, 784)`, values in `[0, 1]`.
- Convert to `float32`, divide by `255.0` exactly once, and flatten in NumPy's default C order without transposing the image.
- A single image requires shape `(1, 784)`.

These transformations are performed outside the network and are **not included** in the saved model. Already normalized validation inputs must not be divided by 255 again. Arbitrary photographs require separate preparation and evaluation; compatibility with Fashion-MNIST inputs does not establish performance on real photographs.

## Output contract

Output shape: `(N, 10)`. Each row contains softmax probabilities in this class order:

| Index | Class |
|---|---|
| 0 | T-shirt/top |
| 1 | Trouser |
| 2 | Pullover |
| 3 | Dress |
| 4 | Coat |
| 5 | Sandal |
| 6 | Shirt |
| 7 | Sneaker |
| 8 | Bag |
| 9 | Ankle boot |

Select the largest probability along the class axis (`axis=1`) to obtain the predicted class index. Softmax scores have not been assessed for calibration.

Class definitions: [official Fashion-MNIST repository](https://github.com/zalandoresearch/fashion-mnist#labels).

## Training configuration

| Setting | Value |
|---|---|
| Dataset loader | Hugging Face `datasets`, `zalando-datasets/fashion_mnist` |
| Backend | JAX, selected before importing Keras |
| Random seed | 42 via `keras.utils.set_random_seed` |
| Optimizer | Adam, notebook defaults |
| Loss | Sparse categorical crossentropy |
| Batch size | 125 |
| Maximum epochs | 20 |
| Validation | Last 12,000 of the 60,000 training examples, indices `48000:60000` |
| Training subset | First 48,000 training examples |
| Early stopping | Monitor `val_loss`, patience 5, restore best weights |

A fixed seed does not guarantee identical results across library versions, platforms, or execution conditions. Dataset revision and the full training environment were not independently frozen for this run.

## Results and evidence

The student reported the following results for the new run:

| Measurement | Result |
|---|---:|
| Best epoch by validation loss | 9 |
| Minimum validation loss | 0.31498199701309204 |
| Validation accuracy | 88.52% |
| Correct / incorrect validation predictions | 10,622 / 1,378 |

The tutor reviewed the saving and comparison code and verified the saved archive on disk: **1,343,018 bytes**, containing `metadata.json`, `config.json`, and `model.weights.h5`.

The student supplied output showing a maximum absolute difference of **0.0** between original and reloaded probabilities on all 12,000 validation images. After preparing raw image `x[48000]` separately, the student also reported a maximum absolute difference of **0** against `rel_probabilities[0]`; the final comparison code/output was not supplied for independent review.

These checks establish agreement on the compared inputs, not a general guarantee across all environments. No new final test-set evaluation was performed in this lesson. The test set had been consulted in an earlier baseline experiment and is not an untouched holdout.

## Inference procedure used successfully

Select JAX before importing Keras in a fresh Python process:

```python
import os
os.environ["KERAS_BACKEND"] = "jax"

import keras
import numpy as np

# Resolve this path relative to the current working directory,
# or replace it with the absolute path to the saved file.
model_path = "fashion-mnist-mini-dropout.keras"
reloaded_model = keras.models.load_model(model_path, compile=False)
```

For an already loaded raw image `raw_image` with shape `(28, 28)`:

```python
image = raw_image.astype("float32").reshape(1, 784) / 255.0
probabilities = np.asarray(reloaded_model(image, training=False))
predicted_labels = np.argmax(probabilities, axis=1)
```

`compile=False` skips restoring the training configuration; structure and learned weights are still loaded. `training=False` selects inference behavior, including inactive Dropout. Inference does not require a new `fit` call.

The notebook's previous model object raised an internal `NoneType` error in the first Dense layer during both `predict` and a direct call. Reloading a new object with `compile=False`, followed by a direct call, worked according to the student. The initial cause and the separate effects of reloading and `compile=False` were not isolated.

The tutor independently loaded the same file in a separate process using **Keras 3.15.1 / JAX 0.11.2** and `compile=False`; direct inference on a synthetic zero input of shape `(1, 784)` returned finite output of shape `(1, 10)`. This was a loading/inference smoke check, not a reproduction of training or an evaluation on the real validation image.

API reference: [Keras model saving and loading](https://keras.io/api/models/model_saving_apis/model_saving_and_loading/).
