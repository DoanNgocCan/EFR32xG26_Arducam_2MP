import tensorflow as tf
from tensorflow.keras import layers, models 
from tensorflow.keras.applications import MobileNet 

from mltk.core.model import (
    MltkModel,
    TrainMixin,
    ImageDatasetMixin,
    EvaluateClassifierMixin
)
from mltk.core.preprocess.image.parallel_generator import ParallelImageDataGenerator

#---------------------------------------------------------------------------------------------------
# *** SỬA LỖI: Định nghĩa hàm với đúng chữ ký (signature) mà MLTK yêu cầu ***
def mobilenet_preprocessing(params, img, **kwargs):
    """
    Hàm chuẩn hóa ảnh cho MobileNet.
    - params: Đối tượng chứa các tham số từ MLTK (chúng ta không dùng đến).
    - img: Dữ liệu ảnh dạng numpy array.
    """
    return (img / 127.5) - 1.0
#---------------------------------------------------------------------------------------------------

class MyFaceModel(
    MltkModel,
    TrainMixin,
    ImageDatasetMixin,
    EvaluateClassifierMixin
):
    pass
my_model = MyFaceModel()
#---------------------------------------------------------------------------------------------------
# 1. Cấu hình chung
my_model.version = 3
my_model.description = 'Face classifier (112x112) using a custom Keras MobileNet'

#---------------------------------------------------------------------------------------------------
# 2. Cấu hình dataset
my_model.dataset = './dataset/dataset' 
my_model.classes = ['face', 'noface']
my_model.class_mode = 'categorical'
my_model.input_shape = (112, 112, 1)
validation_split = 0.2

#---------------------------------------------------------------------------------------------------
# 3. Cấu hình Data Augmentation
my_model.datagen = ParallelImageDataGenerator(
    rotation_range=15,
    width_shift_range=0.1,
    height_shift_range=0.1,
    zoom_range=0.1,
    horizontal_flip=True,
    
    # Truyền hàm đã được định nghĩa với chữ ký đúng
    preprocessing_function=mobilenet_preprocessing,
    
    validation_split=validation_split,
    cores=0.5,
    max_batches_pending=16
)

#---------------------------------------------------------------------------------------------------
# 4. Cấu hình huấn luyện
my_model.epochs = 50 
my_model.batch_size = 64
my_model.optimizer = 'adam'
my_model.metrics = ['accuracy']
my_model.loss = 'categorical_crossentropy'
my_model.class_weights = 'balanced'
my_model.checkpoint['monitor'] = 'val_accuracy'
my_model.reduce_lr_on_plateau = dict(monitor='val_loss', factor=0.5, patience=4, min_lr=1e-6)
my_model.early_stopping = dict(monitor='val_accuracy', patience=12)

#---------------------------------------------------------------------------------------------------
# 5. Cấu hình kiến trúc Model 
def my_model_builder(model: MltkModel) -> tf.keras.Model:
    base_model = MobileNet(
        input_shape=model.input_shape,
        alpha=0.25,
        include_top=False,
        weights=None
    )
    base_model.trainable = True

    x = base_model.output
    x = layers.GlobalAveragePooling2D()(x)
    x = layers.Dropout(0.2)(x)
    outputs = layers.Dense(model.n_classes, activation='softmax')(x)

    keras_model = models.Model(inputs=base_model.input, outputs=outputs)

    keras_model.compile(
        loss=model.loss,
        optimizer=model.optimizer,
        metrics=model.metrics
    )
    return keras_model

my_model.build_model_function = my_model_builder

#---------------------------------------------------------------------------------------------------
# 6. Cấu hình TFLite
my_model.tflite_converter['optimizations'] = [tf.lite.Optimize.DEFAULT]
my_model.tflite_converter['supported_ops'] = [tf.lite.OpsSet.TFLITE_BUILTINS_INT8]
my_model.tflite_converter['inference_input_type'] = tf.int8 
my_model.tflite_converter['inference_output_type'] = tf.int8
my_model.tflite_converter['representative_dataset'] = 'generate'

#---------------------------------------------------------------------------------------------------
# 7. Chạy script
if __name__ == '__main__':
    from mltk import cli
    import mltk.core as mltk_core
    cli.get_logger(verbose=False)
    mltk_core.train_model('train_model', clean=True)
    mltk_core.evaluate_model('train_model', verbose=True)
    mltk_core.profile_model('train_model')
    mltk_core.summarize_model('train_model')