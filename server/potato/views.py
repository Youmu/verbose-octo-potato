import base64
from datetime import timedelta
from django.http import JsonResponse, HttpResponseBadRequest, HttpResponse
from django.views.decorators.http import require_http_methods
from django.utils import timezone
from django.utils.dateparse import parse_datetime
from .models import Message
import json


@require_http_methods(['POST'])
def post_msg(request):
    try:
        payload = json.loads(request.body)
    except Exception:
        return HttpResponseBadRequest('invalid json')

    ts = payload.get('TimeStamp')
    frm = payload.get('From')
    data = payload.get('Data')
    if not ts or not frm or not data:
        return HttpResponseBadRequest('missing fields')

    dt = parse_datetime(ts)
    if dt is None:
        return HttpResponseBadRequest('invalid TimeStamp')
    # ensure timezone-aware
    if timezone.is_naive(dt):
        dt = timezone.make_aware(dt, timezone.utc)

    # store
    msg = Message(TimeStamp=dt, From=frm, Data=data)
    msg.save()
    return JsonResponse({'Index': msg.pk}, status=201)


@require_http_methods(['GET'])
def get_msgs(request):
    now = timezone.now()
    cutoff = now - timedelta(hours=24)
    qs = Message.objects.filter(TimeStamp__gte=cutoff).order_by('pk')
    msgs = [m.to_dict() for m in qs]
    return JsonResponse({'Messages': msgs})


def view_page(request):
    from django.shortcuts import render
    return render(request, 'Potato/view.html')


def msg_handler(request):
    if request.method == 'POST':
        return post_msg(request)
    elif request.method == 'GET':
        return get_msgs(request)
    else:
        return HttpResponse(status=405)
