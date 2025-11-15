# api/models.py
from django.db import models

class UserProfile(models.Model):
    name = models.CharField(max_length=200, blank=True, null=True)
    rfid_uid = models.CharField(max_length=200, unique=True)
    embedding = models.JSONField(blank=True, null=True)
    image = models.ImageField(upload_to="user_images/", blank=True, null=True)

    def __str__(self):
        return self.name or self.rfid_uid


class Attendance(models.Model):
    user = models.ForeignKey(UserProfile, on_delete=models.CASCADE)
    method = models.CharField(max_length=50, default="unknown")   # FIXED
    confidence = models.FloatField(blank=True, null=True)
    timestamp = models.DateTimeField(auto_now_add=True)

    def __str__(self):
        return f"{self.user} @ {self.timestamp}"
