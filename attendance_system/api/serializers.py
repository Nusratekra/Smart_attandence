from rest_framework import serializers
from .models import UserProfile, Attendance

class UserProfileSerializer(serializers.ModelSerializer):
    class Meta:
        model = UserProfile
        fields = ['id', 'name', 'rfid_uid', 'embedding', 'image']

class AttendanceSerializer(serializers.ModelSerializer):
    class Meta:
        model = Attendance
        fields = ['id', 'user', 'method', 'confidence', 'timestamp']
