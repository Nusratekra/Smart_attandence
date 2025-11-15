# api/signals.py
from django.db.models.signals import post_save
from django.dispatch import receiver
from PIL import Image
import numpy as np
import face_recognition
from .models import UserProfile

@receiver(post_save, sender=UserProfile)
def generate_embedding(sender, instance, created, **kwargs):
    if not instance.image or instance.embedding:
        return
    
    # Prevent recursion
    if kwargs.get('update_fields') and 'embedding' in kwargs.get('update_fields'):
        return

    try:
        img = Image.open(instance.image.path)

        # Convert to RGB
        if img.mode != 'RGB':
            if img.mode == 'RGBA':
                background = Image.new('RGB', img.size, (255, 255, 255))
                background.paste(img, mask=img.split()[3])
                img = background
            else:
                img = img.convert('RGB')

        img_np = np.array(img)
        if img_np.dtype != np.uint8:
            img_np = img_np.astype(np.uint8)

        # Generate embedding
        encs = face_recognition.face_encodings(img_np)
        if encs:
            instance.embedding = encs[0].tolist()
            instance.save(update_fields=["embedding"])
            print(f"✓ Embedding generated for {instance}")
        else:
            print(f"⚠ No face detected for {instance}")

    except Exception as e:
        print(f"✗ Embedding generation failed for {instance}: {e}")
