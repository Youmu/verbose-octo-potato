from django.db import models


class Message(models.Model):
    TimeStamp = models.DateTimeField()
    From = models.CharField(max_length=64)
    Data = models.TextField()

    class Meta:
        db_table = 'potato'

    def to_dict(self):
        return {
            'Index': str(self.pk),
            'TimeStamp': self.TimeStamp.isoformat() + 'Z' if self.TimeStamp.tzinfo is None else self.TimeStamp.isoformat(),
            'From': self.From,
            'Data': self.Data,
        }
