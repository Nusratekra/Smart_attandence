# api/views.py
import base64, io
from PIL import Image
import numpy as np
import face_recognition
from rest_framework.decorators import api_view
from rest_framework.response import Response
from .models import UserProfile, Attendance

# 1) UID scan endpoint (called by RFID-ESP32)
@api_view(['POST'])
def uid_scan(request):
    """
    Receives: { "rfid_uid": "..." }
    Response:
      - {"status":"ok","message":"UID exists","action":"capture_required"}
      - {"status":"fail","message":"UID not found"}
    """
    data = request.data
    rfid_uid = data.get('rfid_uid')
    if not rfid_uid:
        return Response({"error": "rfid_uid required"}, status=400)

    rfid_uid = rfid_uid.strip().upper()
    try:
        user = UserProfile.objects.get(rfid_uid=rfid_uid)
        return Response({"status": "ok", "message": "UID exists", "action": "capture_required"})
    except UserProfile.DoesNotExist:
        return Response({"status": "fail", "message": "UID not found"}, status=404)

# 2) Checkin endpoint (called by ESP32-CAM after capture)
@api_view(['POST'])
def checkin(request):
    """
    Receives: {
      "rfid_uid": "...",
      "image_base64": "data:image/jpeg;base64,...."
    }
    Compares face encoding with stored user.embedding and stores an Attendance record.
    """
    data = request.data
    rfid_uid = data.get('rfid_uid')
    image_b64 = data.get('image_base64')

    if not rfid_uid or not image_b64:
        return Response({"error": "rfid_uid and image_base64 required"}, status=400)

    rfid_uid = rfid_uid.strip().upper()
    try:
        user = UserProfile.objects.get(rfid_uid=rfid_uid)
    except UserProfile.DoesNotExist:
        return Response({"status": "fail", "message": "user not found"}, status=404)

    if not user.embedding:
        return Response({"status": "fail", "message": "no stored embedding for user"}, status=400)

    # strip data URL prefix if present
    img_str = image_b64.split(',')[1] if ',' in image_b64 else image_b64
    try:
        image_bytes = base64.b64decode(img_str)
    except Exception:
        return Response({"status": "fail", "message": "invalid base64"}, status=400)

    try:
        pil = Image.open(io.BytesIO(image_bytes)).convert('RGB')
        img_np = np.array(pil)
    except Exception:
        return Response({"status": "fail", "message": "invalid image"}, status=400)

    encodings = face_recognition.face_encodings(img_np)
    if not encodings:
        Attendance.objects.create(user=user, method='rfid+face_failed', confidence=None)
        return Response({"status": "fail", "message": "no face detected"}, status=400)

    captured_enc = encodings[0]
    # ensure numpy array dtype float
    known_enc = np.array(user.embedding, dtype=float)

    # compute distance and match
    dist = face_recognition.face_distance([known_enc], captured_enc)[0]
    # compare_faces uses a tolerance; 0.5 is strict; 0.6 is common default
    match = face_recognition.compare_faces([known_enc], captured_enc, tolerance=0.6)[0]

    # convert distance -> rough confidence score between 0 and 1
    # clamp to [0,1]
    confidence = max(0.0, min(1.0, float(1.0 - dist)))
    confidence = float(confidence)

    if match:
        Attendance.objects.create(user=user, method='rfid+face', confidence=confidence)
        return Response({"status": "success", "message": "attendance recorded", "user": user.name, "confidence": confidence})
    else:
        Attendance.objects.create(user=user, method='rfid+face_failed', confidence=confidence)
        return Response({"status": "fail", "message": "face did not match", "confidence": confidence})
